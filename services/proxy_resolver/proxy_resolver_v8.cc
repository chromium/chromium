// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/proxy_resolver/proxy_resolver_v8.h"

#include <algorithm>
#include <cstdio>
#include <utility>

#include "base/auto_reset.h"
#include "base/compiler_specific.h"
#include "base/debug/leak_annotations.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/stl_util.h"
#include "base/strings/string_tokenizer.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/synchronization/lock.h"
#include "base/threading/thread_task_runner_handle.h"
#include "gin/array_buffer.h"
#include "gin/public/isolate_holder.h"
#include "gin/v8_initializer.h"
#include "net/base/ip_address.h"
#include "net/base/net_errors.h"
#include "net/proxy_resolution/pac_file_data.h"
#include "net/proxy_resolution/proxy_info.h"
#include "services/proxy_resolver/pac_js_library.h"
#include "url/gurl.h"
#include "url/url_canon.h"
#include "v8/include/v8.h"

// Notes on the javascript environment:
//
// For the majority of the PAC utility functions, we use the same code
// as Firefox. See the javascript library that pac_js_library.h pulls in.
//
// In addition, we implement a subset of Microsoft's extensions to PAC.
// - myIpAddressEx()
// - dnsResolveEx()
// - isResolvableEx()
// - isInNetEx()
// - sortIpAddressList()
//
// It is worth noting that the original PAC specification does not describe
// the return values on failure. Consequently, there are compatibility
// differences between browsers on what to return on failure, which are
// illustrated below:
//
// --------------------+-------------+-------------------+--------------
//                     | Firefox3    | InternetExplorer8 |  --> Us <---
// --------------------+-------------+-------------------+--------------
// myIpAddress()       | "127.0.0.1" |  ???              |  "127.0.0.1"
// dnsResolve()        | null        |  false            |  null
// myIpAddressEx()     | N/A         |  ""               |  ""
// sortIpAddressList() | N/A         |  false            |  false
// dnsResolveEx()      | N/A         |  ""               |  ""
// isInNetEx()         | N/A         |  false            |  false
// --------------------+-------------+-------------------+--------------
//
// TODO(eroman): The cell above reading ??? means I didn't test it.
//
// Another difference is in how dnsResolve() and myIpAddress() are
// implemented -- whether they should restrict to IPv4 results, or
// include both IPv4 and IPv6. The following table illustrates the
// differences:
//
// --------------------+-------------+-------------------+--------------
//                     | Firefox3    | InternetExplorer8 |  --> Us <---
// --------------------+-------------+-------------------+--------------
// myIpAddress()       | IPv4/IPv6   |  IPv4             |  IPv4/IPv6
// dnsResolve()        | IPv4/IPv6   |  IPv4             |  IPv4
// isResolvable()      | IPv4/IPv6   |  IPv4             |  IPv4
// myIpAddressEx()     | N/A         |  IPv4/IPv6        |  IPv4/IPv6
// dnsResolveEx()      | N/A         |  IPv4/IPv6        |  IPv4/IPv6
// sortIpAddressList() | N/A         |  IPv4/IPv6        |  IPv4/IPv6
// isResolvableEx()    | N/A         |  IPv4/IPv6        |  IPv4/IPv6
// isInNetEx()         | N/A         |  IPv4/IPv6        |  IPv4/IPv6
// -----------------+-------------+-------------------+--------------

namespace proxy_resolver {

namespace {

// Pseudo-name for the PAC script.
const char kPacResourceName[] = "proxy-pac-script.js";
// Pseudo-name for the PAC utility script.
const char kPacUtilityResourceName[] = "proxy-pac-utility-script.js";

// External string wrapper so V8 can access the UTF16 string wrapped by
// net::PacFileData.
class V8ExternalStringFromScriptData
    : public v8::String::ExternalStringResource {
 public:
  explicit V8ExternalStringFromScriptData(
      const scoped_refptr<net::PacFileData>& script_data)
      : script_data_(script_data) {}

  const uint16_t* data() const override {
    return reinterpret_cast<const uint16_t*>(script_data_->utf16().data());
  }

  size_t length() const override { return script_data_->utf16().size(); }

 private:
  const scoped_refptr<net::PacFileData> script_data_;
  DISALLOW_COPY_AND_ASSIGN(V8ExternalStringFromScriptData);
};

// External string wrapper so V8 can access a string literal.
class V8ExternalASCIILiteral
    : public v8::String::ExternalOneByteStringResource {
 public:
  // |ascii| must be a NULL-terminated C string, and must remain valid
  // throughout this object's lifetime.
  V8ExternalASCIILiteral(const char* ascii, size_t length)
      : ascii_(ascii), length_(length) {
    DCHECK(base::IsStringASCII(ascii));
  }

  const char* data() const override { return ascii_; }

  size_t length() const override { return length_; }

 private:
  const char* ascii_;
  size_t length_;
  DISALLOW_COPY_AND_ASSIGN(V8ExternalASCIILiteral);
};

// When creating a v8::String from a C++ string we have two choices: create
// a copy, or create a wrapper that shares the same underlying storage.
// For small strings it is better to just make a copy, whereas for large
// strings there are savings by sharing the storage. This number identifies
// the cutoff length for when to start wrapping rather than creating copies.
const size_t kMaxStringBytesForCopy = 256;

// Converts a V8 String to a UTF8 std::string.
std::string V8StringToUTF8(v8::Isolate* isolate, v8::Local<v8::String> s) {
  int len = s->Length();
  std::string result;
  if (len > 0)
    s->WriteUtf8(isolate, base::WriteInto(&result, len + 1));
  return result;
}

// Converts a V8 String to a UTF16 base::string16.
base::string16 V8StringToUTF16(v8::Isolate* isolate, v8::Local<v8::String> s) {
  int len = s->Length();
  base::string16 result;
  // Note that the reinterpret cast is because on Windows string16 is an alias
  // to wstring, and hence has character type wchar_t not uint16_t.
  if (len > 0) {
    s->Write(isolate,
             reinterpret_cast<uint16_t*>(base::WriteInto(&result, len + 1)), 0,
             len);
  }
  return result;
}

// Converts an ASCII std::string to a V8 string.
v8::Local<v8::String> ASCIIStringToV8String(v8::Isolate* isolate,
                                            const std::string& s) {
  DCHECK(base::IsStringASCII(s));
  return v8::String::NewFromUtf8(isolate, s.data(), v8::NewStringType::kNormal,
                                 s.size())
      .ToLocalChecked();
}

// Converts a UTF16 base::string16 (wrapped by a net::PacFileData) to a
// V8 string.
v8::Local<v8::String> ScriptDataToV8String(
    v8::Isolate* isolate,
    const scoped_refptr<net::PacFileData>& s) {
  if (s->utf16().size() * 2 <= kMaxStringBytesForCopy) {
    return v8::String::NewFromTwoByte(
               isolate, reinterpret_cast<const uint16_t*>(s->utf16().data()),
               v8::NewStringType::kNormal, s->utf16().size())
        .ToLocalChecked();
  }
  return v8::String::NewExternalTwoByte(isolate,
                                        new V8ExternalStringFromScriptData(s))
      .ToLocalChecked();
}

// Converts an ASCII string literal to a V8 string.
v8::Local<v8::String> ASCIILiteralToV8String(v8::Isolate* isolate,
                                             const char* ascii) {
  DCHECK(base::IsStringASCII(ascii));
  size_t length = strlen(ascii);
  if (length <= kMaxStringBytesForCopy)
    return v8::String::NewFromUtf8(isolate, ascii, v8::NewStringType::kNormal,
                                   length)
        .ToLocalChecked();
  return v8::String::NewExternalOneByte(
             isolate, new V8ExternalASCIILiteral(ascii, length))
      .ToLocalChecked();
}

// Stringizes a V8 object by calling its toString() method. Returns true
// on success. This may fail if the toString() throws an exception.
bool V8ObjectToUTF16String(v8::Local<v8::Value> object,
                           base::string16* utf16_result,
                           v8::Isolate* isolate) {
  if (object.IsEmpty())
    return false;

  v8::HandleScope scope(isolate);
  v8::Local<v8::String> str_object;
  if (!object->ToString(isolate->GetCurrentContext()).ToLocal(&str_object))
    return false;
  *utf16_result = V8StringToUTF16(isolate, str_object);
  return true;
}

// Extracts an hostname argument from |args|. On success returns true
// and fills |*hostname| with the result.
bool GetHostnameArgument(const v8::FunctionCallbackInfo<v8::Value>& args,
                         std::string* hostname) {
  // The first argument should be a string.
  if (args.Length() == 0 || args[0].IsEmpty() || !args[0]->IsString())
    return false;

  const base::string16 hostname_utf16 =
      V8StringToUTF16(args.GetIsolate(), v8::Local<v8::String>::Cast(args[0]));

  // If the hostname is already in ASCII, simply return it as is.
  if (base::IsStringASCII(hostname_utf16)) {
    *hostname = base::UTF16ToASCII(hostname_utf16);
    return true;
  }

  // Otherwise try to convert it from IDN to punycode.
  const int kInitialBufferSize = 256;
  url::RawCanonOutputT<base::char16, kInitialBufferSize> punycode_output;
  if (!url::IDNToASCII(hostname_utf16.data(), hostname_utf16.length(),
                       &punycode_output)) {
    return false;
  }

  // |punycode_output| should now be ASCII; convert it to a std::string.
  // (We could use UTF16ToASCII() instead, but that requires an extra string
  // copy. Since ASCII is a subset of UTF8 the following is equivalent).
  bool success = base::UTF16ToUTF8(punycode_output.data(),
                                   punycode_output.length(), hostname);
  DCHECK(success);
  DCHECK(base::IsStringASCII(*hostname));
  return success;
}

// Wrapper around an IP address that stores the original string as well as a
// corresponding parsed net::IPAddress.

// This struct is used as a helper for sorting IP address strings - the IP
// literal is parsed just once and used as the sorting key, while also
// preserving the original IP literal string.
struct IPAddressSortingEntry {
  IPAddressSortingEntry(const std::string& ip_string,
                        const net::IPAddress& ip_address)
      : string_value(ip_string), ip_address(ip_address) {}

  // Used for sorting IP addresses in ascending order in SortIpAddressList().
  // IPv6 addresses are placed ahead of IPv4 addresses.
  bool operator<(const IPAddressSortingEntry& rhs) const {
    const net::IPAddress& ip1 = this->ip_address;
    const net::IPAddress& ip2 = rhs.ip_address;
    if (ip1.size() != ip2.size())
      return ip1.size() > ip2.size();  // IPv6 before IPv4.
    return ip1 < ip2;                  // Ascending order.
  }

  std::string string_value;
  net::IPAddress ip_address;
};

// Handler for "sortIpAddressList(IpAddressList)". |ip_address_list| is a
// semi-colon delimited string containing IP addresses.
// |sorted_ip_address_list| is the resulting list of sorted semi-colon delimited
// IP addresses or an empty string if unable to sort the IP address list.
// Returns 'true' if the sorting was successful, and 'false' if the input was an
// empty string, a string of separators (";" in this case), or if any of the IP
// addresses in the input list failed to parse.
bool SortIpAddressList(const std::string& ip_address_list,
                       std::string* sorted_ip_address_list) {
  sorted_ip_address_list->clear();

  // Strip all whitespace (mimics IE behavior).
  std::string cleaned_ip_address_list;
  base::RemoveChars(ip_address_list, " \t", &cleaned_ip_address_list);
  if (cleaned_ip_address_list.empty())
    return false;

  // Split-up IP addresses and store them in a vector.
  std::vector<IPAddressSortingEntry> ip_vector;
  net::IPAddress ip_address;
  base::StringTokenizer str_tok(cleaned_ip_address_list, ";");
  while (str_tok.GetNext()) {
    if (!ip_address.AssignFromIPLiteral(str_tok.token()))
      return false;
    ip_vector.push_back(IPAddressSortingEntry(str_tok.token(), ip_address));
  }

  if (ip_vector.empty())  // Can happen if we have something like
    return false;         // sortIpAddressList(";") or sortIpAddressList("; ;")

  DCHECK(!ip_vector.empty());

  // Sort lists according to ascending numeric value.
  if (ip_vector.size() > 1)
    std::stable_sort(ip_vector.begin(), ip_vector.end());

  // Return a semi-colon delimited list of sorted addresses (IPv6 followed by
  // IPv4).
  for (size_t i = 0; i < ip_vector.size(); ++i) {
    if (i > 0)
      *sorted_ip_address_list += ";";
    *sorted_ip_address_list += ip_vector[i].string_value;
  }
  return true;
}

// Handler for "isInNetEx(ip_address, ip_prefix)". |ip_address| is a string
// containing an IPv4/IPv6 address, and |ip_prefix| is a string containg a
// slash-delimited IP prefix with the top 'n' bits specified in the bit
// field. This returns 'true' if the address is in the same subnet, and
// 'false' otherwise. Also returns 'false' if the prefix is in an incorrect
// format. If the address types of |ip_address| and |ip_prefix| don't match,
// will promote the IPv4 literal to an IPv4 mapped IPv6 literal and
// proceed with the comparison.
bool IsInNetEx(const std::string& ip_address, const std::string& ip_prefix) {
  net::IPAddress address;
  if (!address.AssignFromIPLiteral(ip_address))
    return false;

  net::IPAddress prefix;
  size_t prefix_length_in_bits;
  if (!ParseCIDRBlock(ip_prefix, &prefix, &prefix_length_in_bits))
    return false;

  return IPAddressMatchesPrefix(address, prefix, prefix_length_in_bits);
}

// Consider only single component domains like 'foo' as plain host names.
bool IsPlainHostName(const std::string& hostname_utf8) {
  if (hostname_utf8.find('.') != std::string::npos)
    return false;

  // IPv6 literals might not contain any periods, however are not considered
  // plain host names.
  net::IPAddress unused;
  return !unused.AssignFromIPLiteral(hostname_utf8);
}

// All instances of ProxyResolverV8 share the same v8::Isolate. This isolate is
// created lazily the first time it is needed and lives until process shutdown.
// This creation might happen from any thread, as ProxyResolverV8 is typically
// run in a threadpool.
//
// TODO(eroman): The lazily created isolate is never freed. Instead it should be
// disposed once there are no longer any ProxyResolverV8 referencing it.
class SharedIsolateFactory {
 public:
  SharedIsolateFactory() : has_initialized_v8_(false) {}

  // Lazily creates a v8::Isolate, or returns the already created instance.
  v8::Isolate* GetSharedIsolate() {
    base::AutoLock lock(lock_);

    if (!holder_) {
      // Do one-time initialization for V8.
      if (!has_initialized_v8_) {
#ifdef V8_USE_EXTERNAL_STARTUP_DATA
        gin::V8Initializer::LoadV8Snapshot();
#endif

        // The performance of the proxy resolver is limited by DNS resolution,
        // and not V8, so tune down V8 to use as little memory as possible.
        static const char kOptimizeForSize[] = "--optimize_for_size";
        v8::V8::SetFlagsFromString(kOptimizeForSize, strlen(kOptimizeForSize));
        static const char kNoOpt[] = "--noopt";
        v8::V8::SetFlagsFromString(kNoOpt, strlen(kNoOpt));

        // WebAssembly isn't encountered during resolution, so reduce the
        // potential attack surface.
        static const char kNoExposeWasm[] = "--no-expose-wasm";
        v8::V8::SetFlagsFromString(kNoExposeWasm, strlen(kNoExposeWasm));

        gin::IsolateHolder::Initialize(
            gin::IsolateHolder::kNonStrictMode,
            gin::ArrayBufferAllocator::SharedInstance());

        has_initialized_v8_ = true;
      }

      holder_.reset(new gin::IsolateHolder(
          base::ThreadTaskRunnerHandle::Get(), gin::IsolateHolder::kUseLocker,
          gin::IsolateHolder::IsolateType::kUtility));
    }

    return holder_->isolate();
  }

  v8::Isolate* GetSharedIsolateWithoutCreating() {
    base::AutoLock lock(lock_);
    return holder_ ? holder_->isolate() : nullptr;
  }

 private:
  base::Lock lock_;
  std::unique_ptr<gin::IsolateHolder> holder_;
  bool has_initialized_v8_;

  DISALLOW_COPY_AND_ASSIGN(SharedIsolateFactory);
};

base::LazyInstance<SharedIsolateFactory>::Leaky g_isolate_factory =
    LAZY_INSTANCE_INITIALIZER;

}  // namespace

// ProxyResolverV8::Context ---------------------------------------------------

class ProxyResolverV8::Context {
 public:
  explicit Context(v8::Isolate* isolate)
      : js_bindings_(nullptr), isolate_(isolate) {
    DCHECK(isolate);
  }

  ~Context() {
    v8::Locker locked(isolate_);
    v8::Isolate::Scope isolate_scope(isolate_);

    v8_this_.Reset();
    v8_context_.Reset();
  }

  JSBindings* js_bindings() { return js_bindings_; }

  int ResolveProxy(const GURL& query_url,
                   net::ProxyInfo* results,
                   JSBindings* bindings) {
    DCHECK(bindings);
    base::AutoReset<JSBindings*> bindings_reset(&js_bindings_, bindings);
    v8::Locker locked(isolate_);
    v8::Isolate::Scope isolate_scope(isolate_);
    v8::Isolate::SafeForTerminationScope safe_for_termination(isolate_);
    v8::HandleScope scope(isolate_);

    v8::Local<v8::Context> context =
        v8::Local<v8::Context>::New(isolate_, v8_context_);
    v8::Context::Scope function_scope(context);

    v8::Local<v8::Value> function;
    int rv = GetFindProxyForURL(&function);
    if (rv != net::OK)
      return rv;

    v8::Local<v8::Value> argv[] = {
        ASCIIStringToV8String(isolate_, query_url.spec()),
        ASCIIStringToV8String(isolate_, query_url.HostNoBrackets()),
    };

    v8::TryCatch try_catch(isolate_);
    v8::Local<v8::Value> ret;
    if (!v8::Function::Cast(*function)
             ->Call(context, context->Global(), base::size(argv), argv)
             .ToLocal(&ret)) {
      DCHECK(try_catch.HasCaught());
      HandleError(try_catch.Message());
      return net::ERR_PAC_SCRIPT_FAILED;
    }

    if (!ret->IsString()) {
      js_bindings()->OnError(
          -1, base::ASCIIToUTF16("FindProxyForURL() did not return a string."));
      return net::ERR_PAC_SCRIPT_FAILED;
    }

    base::string16 ret_str =
        V8StringToUTF16(isolate_, v8::Local<v8::String>::Cast(ret));

    if (!base::IsStringASCII(ret_str)) {
      // TODO(eroman): Rather than failing when a wide string is returned, we
      //               could extend the parsing to handle IDNA hostnames by
      //               converting them to ASCII punycode.
      //               crbug.com/47234
      base::string16 error_message =
          base::ASCIIToUTF16(
              "FindProxyForURL() returned a non-ASCII string "
              "(crbug.com/47234): ") +
          ret_str;
      js_bindings()->OnError(-1, error_message);
      return net::ERR_PAC_SCRIPT_FAILED;
    }

    results->UsePacString(base::UTF16ToASCII(ret_str));
    return net::OK;
  }

  int InitV8(const scoped_refptr<net::PacFileData>& pac_script,
             JSBindings* bindings) {
    base::AutoReset<JSBindings*> bindings_reset(&js_bindings_, bindings);
    v8::Locker locked(isolate_);
    v8::Isolate::Scope isolate_scope(isolate_);
    v8::HandleScope scope(isolate_);

    v8_this_.Reset(isolate_, v8::External::New(isolate_, this));
    v8::Local<v8::External> v8_this =
        v8::Local<v8::External>::New(isolate_, v8_this_);
    v8::Local<v8::ObjectTemplate> global_template =
        v8::ObjectTemplate::New(isolate_);

    // Attach the javascript bindings.
    v8::Local<v8::FunctionTemplate> alert_template =
        v8::FunctionTemplate::New(isolate_, &AlertCallback, v8_this);
    alert_template->RemovePrototype();
    global_template->Set(ASCIILiteralToV8String(isolate_, "alert"),
                         alert_template);

    v8::Local<v8::FunctionTemplate> my_ip_address_template =
        v8::FunctionTemplate::New(isolate_, &MyIpAddressCallback, v8_this);
    my_ip_address_template->RemovePrototype();
    global_template->Set(ASCIILiteralToV8String(isolate_, "myIpAddress"),
                         my_ip_address_template);

    v8::Local<v8::FunctionTemplate> dns_resolve_template =
        v8::FunctionTemplate::New(isolate_, &DnsResolveCallback, v8_this);
    dns_resolve_template->RemovePrototype();
    global_template->Set(ASCIILiteralToV8String(isolate_, "dnsResolve"),
                         dns_resolve_template);

    v8::Local<v8::FunctionTemplate> is_plain_host_name_template =
        v8::FunctionTemplate::New(isolate_, &IsPlainHostNameCallback, v8_this);
    is_plain_host_name_template->RemovePrototype();
    global_template->Set(ASCIILiteralToV8String(isolate_, "isPlainHostName"),
                         is_plain_host_name_template);

    // Microsoft's PAC extensions:

    v8::Local<v8::FunctionTemplate> dns_resolve_ex_template =
        v8::FunctionTemplate::New(isolate_, &DnsResolveExCallback, v8_this);
    dns_resolve_ex_template->RemovePrototype();
    global_template->Set(ASCIILiteralToV8String(isolate_, "dnsResolveEx"),
                         dns_resolve_ex_template);

    v8::Local<v8::FunctionTemplate> my_ip_address_ex_template =
        v8::FunctionTemplate::New(isolate_, &MyIpAddressExCallback, v8_this);
    my_ip_address_ex_template->RemovePrototype();
    global_template->Set(ASCIILiteralToV8String(isolate_, "myIpAddressEx"),
                         my_ip_address_ex_template);

    v8::Local<v8::FunctionTemplate> sort_ip_address_list_template =
        v8::FunctionTemplate::New(isolate_, &SortIpAddressListCallback,
                                  v8_this);
    sort_ip_address_list_template->RemovePrototype();
    global_template->Set(ASCIILiteralToV8String(isolate_, "sortIpAddressList"),
                         sort_ip_address_list_template);

    v8::Local<v8::FunctionTemplate> is_in_net_ex_template =
        v8::FunctionTemplate::New(isolate_, &IsInNetExCallback, v8_this);
    is_in_net_ex_template->RemovePrototype();
    global_template->Set(ASCIILiteralToV8String(isolate_, "isInNetEx"),
                         is_in_net_ex_template);

    v8_context_.Reset(isolate_,
                      v8::Context::New(isolate_, nullptr, global_template));

    v8::Local<v8::Context> context =
        v8::Local<v8::Context>::New(isolate_, v8_context_);
    v8::Context::Scope ctx(context);

    // Add the PAC utility functions to the environment.
    // (This script should never fail, as it is a string literal!)
    // Note that the two string literals are concatenated.
    int rv = RunScript(
        ASCIILiteralToV8String(isolate_, PAC_JS_LIBRARY PAC_JS_LIBRARY_EX),
        kPacUtilityResourceName);
    if (rv != net::OK) {
      NOTREACHED();
      return rv;
    }

    // Add the user's PAC code to the environment.
    rv =
        RunScript(ScriptDataToV8String(isolate_, pac_script), kPacResourceName);
    if (rv != net::OK)
      return rv;

    // At a minimum, the FindProxyForURL() function must be defined for this
    // to be a legitimiate PAC script.
    v8::Local<v8::Value> function;
    return GetFindProxyForURL(&function);
  }

 private:
  int GetFindProxyForURL(v8::Local<v8::Value>* function) {
    v8::Local<v8::Context> context =
        v8::Local<v8::Context>::New(isolate_, v8_context_);

    v8::TryCatch try_catch(isolate_);

    if (!context->Global()
             ->Get(context, ASCIILiteralToV8String(isolate_, "FindProxyForURL"))
             .ToLocal(function)) {
      DCHECK(try_catch.HasCaught());
      HandleError(try_catch.Message());
    }

    // The value should only be empty if an exception was thrown. Code
    // defensively just in case.
    DCHECK_EQ(function->IsEmpty(), try_catch.HasCaught());
    if (function->IsEmpty() || try_catch.HasCaught()) {
      js_bindings()->OnError(
          -1,
          base::ASCIIToUTF16("Accessing FindProxyForURL threw an exception."));
      return net::ERR_PAC_SCRIPT_FAILED;
    }

    if (!(*function)->IsFunction()) {
      js_bindings()->OnError(
          -1, base::ASCIIToUTF16(
                  "FindProxyForURL is undefined or not a function."));
      return net::ERR_PAC_SCRIPT_FAILED;
    }

    return net::OK;
  }

  // Handle an exception thrown by V8.
  void HandleError(v8::Local<v8::Message> message) {
    v8::Local<v8::Context> context =
        v8::Local<v8::Context>::New(isolate_, v8_context_);
    base::string16 error_message;
    int line_number = -1;

    if (!message.IsEmpty()) {
      auto maybe = message->GetLineNumber(context);
      if (maybe.IsJust())
        line_number = maybe.FromJust();
      V8ObjectToUTF16String(message->Get(), &error_message, isolate_);
    }

    js_bindings()->OnError(line_number, error_message);
  }

  // Compiles and runs |script| in the current V8 context.
  // Returns net::OK on success, otherwise an error code.
  int RunScript(v8::Local<v8::String> script, const char* script_name) {
    v8::Local<v8::Context> context =
        v8::Local<v8::Context>::New(isolate_, v8_context_);
    v8::TryCatch try_catch(isolate_);

    // Compile the script.
    v8::ScriptOrigin origin =
        v8::ScriptOrigin(ASCIILiteralToV8String(isolate_, script_name));
    v8::ScriptCompiler::Source script_source(script, origin);
    v8::Local<v8::Script> code;
    if (!v8::ScriptCompiler::Compile(
             context, &script_source, v8::ScriptCompiler::kNoCompileOptions,
             v8::ScriptCompiler::NoCacheReason::kNoCacheBecausePacScript)
             .ToLocal(&code)) {
      DCHECK(try_catch.HasCaught());
      HandleError(try_catch.Message());
      return net::ERR_PAC_SCRIPT_FAILED;
    }

    // Execute.
    auto result = code->Run(context);
    if (result.IsEmpty()) {
      DCHECK(try_catch.HasCaught());
      HandleError(try_catch.Message());
      return net::ERR_PAC_SCRIPT_FAILED;
    }

    return net::OK;
  }

  // V8 callback for when "alert()" is invoked by the PAC script.
  static void AlertCallback(const v8::FunctionCallbackInfo<v8::Value>& args) {
    Context* context =
        static_cast<Context*>(v8::External::Cast(*args.Data())->Value());

    // Like firefox we assume "undefined" if no argument was specified, and
    // disregard any arguments beyond the first.
    base::string16 message;
    if (args.Length() == 0) {
      message = base::ASCIIToUTF16("undefined");
    } else {
      if (!V8ObjectToUTF16String(args[0], &message, args.GetIsolate()))
        return;  // toString() threw an exception.
    }

    context->js_bindings()->Alert(message);
  }

  // V8 callback for when "myIpAddress()" is invoked by the PAC script.
  static void MyIpAddressCallback(
      const v8::FunctionCallbackInfo<v8::Value>& args) {
    DnsResolveCallbackHelper(args,
                             net::ProxyResolveDnsOperation::MY_IP_ADDRESS);
  }

  // V8 callback for when "myIpAddressEx()" is invoked by the PAC script.
  static void MyIpAddressExCallback(
      const v8::FunctionCallbackInfo<v8::Value>& args) {
    DnsResolveCallbackHelper(args,
                             net::ProxyResolveDnsOperation::MY_IP_ADDRESS_EX);
  }

  // V8 callback for when "dnsResolve()" is invoked by the PAC script.
  static void DnsResolveCallback(
      const v8::FunctionCallbackInfo<v8::Value>& args) {
    DnsResolveCallbackHelper(args, net::ProxyResolveDnsOperation::DNS_RESOLVE);
  }

  // V8 callback for when "dnsResolveEx()" is invoked by the PAC script.
  static void DnsResolveExCallback(
      const v8::FunctionCallbackInfo<v8::Value>& args) {
    DnsResolveCallbackHelper(args,
                             net::ProxyResolveDnsOperation::DNS_RESOLVE_EX);
  }

  // Shared code for implementing:
  //   - myIpAddress(), myIpAddressEx(), dnsResolve(), dnsResolveEx().
  static void DnsResolveCallbackHelper(
      const v8::FunctionCallbackInfo<v8::Value>& args,
      net::ProxyResolveDnsOperation op) {
    Context* context =
        static_cast<Context*>(v8::External::Cast(*args.Data())->Value());

    std::string hostname;

    // dnsResolve() and dnsResolveEx() need at least 1 argument.
    if (op == net::ProxyResolveDnsOperation::DNS_RESOLVE ||
        op == net::ProxyResolveDnsOperation::DNS_RESOLVE_EX) {
      if (!GetHostnameArgument(args, &hostname)) {
        if (op == net::ProxyResolveDnsOperation::DNS_RESOLVE)
          args.GetReturnValue().SetNull();
        return;
      }
    }

    std::string result;
    bool success;
    bool terminate = false;

    {
      v8::Unlocker unlocker(args.GetIsolate());
      success =
          context->js_bindings()->ResolveDns(hostname, op, &result, &terminate);
    }

    if (terminate)
      args.GetIsolate()->TerminateExecution();

    if (success) {
      args.GetReturnValue().Set(
          ASCIIStringToV8String(args.GetIsolate(), result));
      return;
    }

    // Each function handles resolution errors differently.
    switch (op) {
      case net::ProxyResolveDnsOperation::DNS_RESOLVE:
        args.GetReturnValue().SetNull();
        return;
      case net::ProxyResolveDnsOperation::DNS_RESOLVE_EX:
        args.GetReturnValue().SetEmptyString();
        return;
      case net::ProxyResolveDnsOperation::MY_IP_ADDRESS:
        args.GetReturnValue().Set(
            ASCIILiteralToV8String(args.GetIsolate(), "127.0.0.1"));
        return;
      case net::ProxyResolveDnsOperation::MY_IP_ADDRESS_EX:
        args.GetReturnValue().SetEmptyString();
        return;
    }

    NOTREACHED();
  }

  // V8 callback for when "sortIpAddressList()" is invoked by the PAC script.
  static void SortIpAddressListCallback(
      const v8::FunctionCallbackInfo<v8::Value>& args) {
    // We need at least one string argument.
    if (args.Length() == 0 || args[0].IsEmpty() || !args[0]->IsString()) {
      args.GetReturnValue().SetNull();
      return;
    }

    std::string ip_address_list =
        V8StringToUTF8(args.GetIsolate(), v8::Local<v8::String>::Cast(args[0]));
    if (!base::IsStringASCII(ip_address_list)) {
      args.GetReturnValue().SetNull();
      return;
    }
    std::string sorted_ip_address_list;
    bool success = SortIpAddressList(ip_address_list, &sorted_ip_address_list);
    if (!success) {
      args.GetReturnValue().Set(false);
      return;
    }
    args.GetReturnValue().Set(
        ASCIIStringToV8String(args.GetIsolate(), sorted_ip_address_list));
  }

  // V8 callback for when "isInNetEx()" is invoked by the PAC script.
  static void IsInNetExCallback(
      const v8::FunctionCallbackInfo<v8::Value>& args) {
    // We need at least 2 string arguments.
    if (args.Length() < 2 || args[0].IsEmpty() || !args[0]->IsString() ||
        args[1].IsEmpty() || !args[1]->IsString()) {
      args.GetReturnValue().SetNull();
      return;
    }

    std::string ip_address =
        V8StringToUTF8(args.GetIsolate(), v8::Local<v8::String>::Cast(args[0]));
    if (!base::IsStringASCII(ip_address)) {
      args.GetReturnValue().Set(false);
      return;
    }
    std::string ip_prefix =
        V8StringToUTF8(args.GetIsolate(), v8::Local<v8::String>::Cast(args[1]));
    if (!base::IsStringASCII(ip_prefix)) {
      args.GetReturnValue().Set(false);
      return;
    }
    args.GetReturnValue().Set(IsInNetEx(ip_address, ip_prefix));
  }

  // V8 callback for when "isPlainHostName()" is invoked by the PAC script.
  static void IsPlainHostNameCallback(
      const v8::FunctionCallbackInfo<v8::Value>& args) {
    // Need at least 1 string arguments.
    if (args.Length() < 1 || args[0].IsEmpty() || !args[0]->IsString()) {
      args.GetIsolate()->ThrowException(
          v8::Exception::TypeError(ASCIIStringToV8String(
              args.GetIsolate(), "Requires 1 string parameter")));
      return;
    }

    std::string hostname_utf8 =
        V8StringToUTF8(args.GetIsolate(), v8::Local<v8::String>::Cast(args[0]));
    args.GetReturnValue().Set(IsPlainHostName(hostname_utf8));
  }

  mutable base::Lock lock_;
  ProxyResolverV8::JSBindings* js_bindings_;
  v8::Isolate* isolate_;
  v8::Persistent<v8::External> v8_this_;
  v8::Persistent<v8::Context> v8_context_;
};

// ProxyResolverV8 ------------------------------------------------------------

ProxyResolverV8::ProxyResolverV8(std::unique_ptr<Context> context)
    : context_(std::move(context)) {
  DCHECK(context_);
}

ProxyResolverV8::~ProxyResolverV8() = default;

int ProxyResolverV8::GetProxyForURL(const GURL& query_url,
                                    net::ProxyInfo* results,
                                    ProxyResolverV8::JSBindings* bindings) {
  return context_->ResolveProxy(query_url, results, bindings);
}

// static
int ProxyResolverV8::Create(const scoped_refptr<net::PacFileData>& script_data,
                            ProxyResolverV8::JSBindings* js_bindings,
                            std::unique_ptr<ProxyResolverV8>* resolver) {
  DCHECK(script_data.get());
  DCHECK(js_bindings);

  if (script_data->utf16().empty())
    return net::ERR_PAC_SCRIPT_FAILED;

  // Try parsing the PAC script.
  std::unique_ptr<Context> context(
      new Context(g_isolate_factory.Get().GetSharedIsolate()));
  int rv = context->InitV8(script_data, js_bindings);
  if (rv == net::OK)
    resolver->reset(new ProxyResolverV8(std::move(context)));
  return rv;
}

// static
size_t ProxyResolverV8::GetTotalHeapSize() {
  v8::Isolate* isolate =
      g_isolate_factory.Get().GetSharedIsolateWithoutCreating();
  if (!isolate)
    return 0;

  v8::Locker locked(isolate);
  v8::Isolate::Scope isolate_scope(isolate);
  v8::HeapStatistics heap_statistics;
  isolate->GetHeapStatistics(&heap_statistics);
  return heap_statistics.total_heap_size();
}

// static
size_t ProxyResolverV8::GetUsedHeapSize() {
  v8::Isolate* isolate =
      g_isolate_factory.Get().GetSharedIsolateWithoutCreating();
  if (!isolate)
    return 0;

  v8::Locker locked(isolate);
  v8::Isolate::Scope isolate_scope(isolate);
  v8::HeapStatistics heap_statistics;
  isolate->GetHeapStatistics(&heap_statistics);
  return heap_statistics.used_heap_size();
}

}  // namespace proxy_resolver
