// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/proxy_resolver/proxy_resolver_v8.h"

#include "base/compiler_specific.h"
#include "base/files/file_util.h"
#include "base/path_service.h"
#include "base/stl_util.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/task_environment.h"
#include "net/base/net_errors.h"
#include "net/proxy_resolution/pac_file_data.h"
#include "net/proxy_resolution/proxy_info.h"
#include "net/test/gtest_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using net::test::IsError;
using net::test::IsOk;
using ::testing::IsEmpty;

namespace proxy_resolver {
namespace {

// Javascript bindings for ProxyResolverV8, which returns mock values.
// Each time one of the bindings is called into, we push the input into a
// list, for later verification.
class MockJSBindings : public ProxyResolverV8::JSBindings {
 public:
  MockJSBindings()
      : my_ip_address_count(0),
        my_ip_address_ex_count(0),
        should_terminate(false) {}

  void Alert(const base::string16& message) override {
    VLOG(1) << "PAC-alert: " << message;  // Helpful when debugging.
    alerts.push_back(base::UTF16ToUTF8(message));
  }

  bool ResolveDns(const std::string& host,
                  net::ProxyResolveDnsOperation op,
                  std::string* output,
                  bool* terminate) override {
    *terminate = should_terminate;

    if (op == net::ProxyResolveDnsOperation::MY_IP_ADDRESS) {
      my_ip_address_count++;
      *output = my_ip_address_result;
      return !my_ip_address_result.empty();
    }

    if (op == net::ProxyResolveDnsOperation::MY_IP_ADDRESS_EX) {
      my_ip_address_ex_count++;
      *output = my_ip_address_ex_result;
      return !my_ip_address_ex_result.empty();
    }

    if (op == net::ProxyResolveDnsOperation::DNS_RESOLVE) {
      dns_resolves.push_back(host);
      *output = dns_resolve_result;
      return !dns_resolve_result.empty();
    }

    if (op == net::ProxyResolveDnsOperation::DNS_RESOLVE_EX) {
      dns_resolves_ex.push_back(host);
      *output = dns_resolve_ex_result;
      return !dns_resolve_ex_result.empty();
    }

    CHECK(false);
    return false;
  }

  void OnError(int line_number, const base::string16& message) override {
    // Helpful when debugging.
    VLOG(1) << "PAC-error: [" << line_number << "] " << message;

    errors.push_back(base::UTF16ToUTF8(message));
    errors_line_number.push_back(line_number);
  }

  // Mock values to return.
  std::string my_ip_address_result;
  std::string my_ip_address_ex_result;
  std::string dns_resolve_result;
  std::string dns_resolve_ex_result;

  // Inputs we got called with.
  std::vector<std::string> alerts;
  std::vector<std::string> errors;
  std::vector<int> errors_line_number;
  std::vector<std::string> dns_resolves;
  std::vector<std::string> dns_resolves_ex;
  int my_ip_address_count;
  int my_ip_address_ex_count;

  // Whether ResolveDns() should terminate script execution.
  bool should_terminate;
};

class ProxyResolverV8Test : public testing::Test {
 public:
  // Creates a ProxyResolverV8 using the PAC script contained in |filename|. If
  // called more than once, the previous ProxyResolverV8 is deleted.
  int CreateResolver(const char* filename) {
    base::FilePath path;
    base::PathService::Get(base::DIR_SOURCE_ROOT, &path);
    path = path.AppendASCII("services");
    path = path.AppendASCII("proxy_resolver");
    path = path.AppendASCII("test");
    path = path.AppendASCII("data");
    path = path.AppendASCII("proxy_resolver_v8_unittest");
    path = path.AppendASCII(filename);

    // Try to read the file from disk.
    std::string file_contents;
    bool ok = base::ReadFileToString(path, &file_contents);

    // If we can't load the file from disk, something is misconfigured.
    if (!ok) {
      LOG(ERROR) << "Failed to read file: " << path.value();
      return net::ERR_FAILED;
    }

    // Create the ProxyResolver using the PAC script.
    return ProxyResolverV8::Create(net::PacFileData::FromUTF8(file_contents),
                                   bindings(), &resolver_);
  }

  ProxyResolverV8& resolver() {
    DCHECK(resolver_);
    return *resolver_;
  }

  MockJSBindings* bindings() { return &js_bindings_; }

 private:
  base::test::TaskEnvironment task_environment_;
  MockJSBindings js_bindings_;
  std::unique_ptr<ProxyResolverV8> resolver_;
};

// Doesn't really matter what these values are for many of the tests.
const GURL kQueryUrl("http://www.google.com");
const GURL kPacUrl;

TEST_F(ProxyResolverV8Test, Direct) {
  ASSERT_THAT(CreateResolver("direct.js"), IsOk());

  net::ProxyInfo proxy_info;
  int result = resolver().GetProxyForURL(kQueryUrl, &proxy_info, bindings());

  EXPECT_THAT(result, IsOk());
  EXPECT_TRUE(proxy_info.is_direct());

  EXPECT_EQ(0U, bindings()->alerts.size());
  EXPECT_EQ(0U, bindings()->errors.size());
}

TEST_F(ProxyResolverV8Test, ReturnEmptyString) {
  ASSERT_THAT(CreateResolver("return_empty_string.js"), IsOk());

  net::ProxyInfo proxy_info;
  int result = resolver().GetProxyForURL(kQueryUrl, &proxy_info, bindings());

  EXPECT_THAT(result, IsOk());
  EXPECT_TRUE(proxy_info.is_direct());

  EXPECT_EQ(0U, bindings()->alerts.size());
  EXPECT_EQ(0U, bindings()->errors.size());
}

TEST_F(ProxyResolverV8Test, Basic) {
  ASSERT_THAT(CreateResolver("passthrough.js"), IsOk());

  // The "FindProxyForURL" of this PAC script simply concatenates all of the
  // arguments into a pseudo-host. The purpose of this test is to verify that
  // the correct arguments are being passed to FindProxyForURL().
  {
    net::ProxyInfo proxy_info;
    int result = resolver().GetProxyForURL(GURL("http://query.com/path"),
                                           &proxy_info, bindings());
    EXPECT_THAT(result, IsOk());
    EXPECT_EQ("http.query.com.path.query.com:80",
              proxy_info.proxy_server().ToURI());
  }
  {
    net::ProxyInfo proxy_info;
    int result = resolver().GetProxyForURL(GURL("ftp://query.com:90/path"),
                                           &proxy_info, bindings());
    EXPECT_THAT(result, IsOk());
    // Note that FindProxyForURL(url, host) does not expect |host| to contain
    // the port number.
    EXPECT_EQ("ftp.query.com.90.path.query.com:80",
              proxy_info.proxy_server().ToURI());

    EXPECT_EQ(0U, bindings()->alerts.size());
    EXPECT_EQ(0U, bindings()->errors.size());
  }
}

TEST_F(ProxyResolverV8Test, BadReturnType) {
  // These are the filenames of PAC scripts which each return a non-string
  // types for FindProxyForURL(). They should all fail with
  // net::ERR_PAC_SCRIPT_FAILED.
  static const char* const filenames[] = {
      "return_undefined.js", "return_integer.js", "return_function.js",
      "return_object.js",
      // TODO(eroman): Should 'null' be considered equivalent to "DIRECT" ?
      "return_null.js"};

  for (size_t i = 0; i < base::size(filenames); ++i) {
    ASSERT_THAT(CreateResolver(filenames[i]), IsOk());

    MockJSBindings bindings;
    net::ProxyInfo proxy_info;
    int result = resolver().GetProxyForURL(kQueryUrl, &proxy_info, &bindings);

    EXPECT_THAT(result, IsError(net::ERR_PAC_SCRIPT_FAILED));

    EXPECT_EQ(0U, bindings.alerts.size());
    ASSERT_EQ(1U, bindings.errors.size());
    EXPECT_EQ("FindProxyForURL() did not return a string.", bindings.errors[0]);
    EXPECT_EQ(-1, bindings.errors_line_number[0]);
  }
}

// Try using a PAC script which defines no "FindProxyForURL" function.
TEST_F(ProxyResolverV8Test, NoEntryPoint) {
  EXPECT_THAT(CreateResolver("no_entrypoint.js"),
              IsError(net::ERR_PAC_SCRIPT_FAILED));

  ASSERT_EQ(1U, bindings()->errors.size());
  EXPECT_EQ("FindProxyForURL is undefined or not a function.",
            bindings()->errors[0]);
  EXPECT_EQ(-1, bindings()->errors_line_number[0]);
}

// Try loading a malformed PAC script.
TEST_F(ProxyResolverV8Test, ParseError) {
  EXPECT_THAT(CreateResolver("missing_close_brace.js"),
              IsError(net::ERR_PAC_SCRIPT_FAILED));

  EXPECT_EQ(0U, bindings()->alerts.size());

  // We get one error during compilation.
  ASSERT_EQ(1U, bindings()->errors.size());

  EXPECT_EQ("Uncaught SyntaxError: Unexpected end of input",
            bindings()->errors[0]);
  EXPECT_EQ(7, bindings()->errors_line_number[0]);
}

// Run a PAC script several times, which has side-effects.
TEST_F(ProxyResolverV8Test, SideEffects) {
  ASSERT_THAT(CreateResolver("side_effects.js"), IsOk());

  // The PAC script increments a counter each time we invoke it.
  for (int i = 0; i < 3; ++i) {
    net::ProxyInfo proxy_info;
    int result = resolver().GetProxyForURL(kQueryUrl, &proxy_info, bindings());
    EXPECT_THAT(result, IsOk());
    EXPECT_EQ(base::StringPrintf("sideffect_%d:80", i),
              proxy_info.proxy_server().ToURI());
  }

  // Reload the script -- the javascript environment should be reset, hence
  // the counter starts over.
  ASSERT_THAT(CreateResolver("side_effects.js"), IsOk());

  for (int i = 0; i < 3; ++i) {
    net::ProxyInfo proxy_info;
    int result = resolver().GetProxyForURL(kQueryUrl, &proxy_info, bindings());
    EXPECT_THAT(result, IsOk());
    EXPECT_EQ(base::StringPrintf("sideffect_%d:80", i),
              proxy_info.proxy_server().ToURI());
  }
}

// Execute a PAC script which throws an exception in FindProxyForURL.
TEST_F(ProxyResolverV8Test, UnhandledException) {
  ASSERT_THAT(CreateResolver("unhandled_exception.js"), IsOk());

  net::ProxyInfo proxy_info;
  int result = resolver().GetProxyForURL(kQueryUrl, &proxy_info, bindings());

  EXPECT_THAT(result, IsError(net::ERR_PAC_SCRIPT_FAILED));

  EXPECT_EQ(0U, bindings()->alerts.size());
  ASSERT_EQ(1U, bindings()->errors.size());
  EXPECT_EQ("Uncaught ReferenceError: undefined_variable is not defined",
            bindings()->errors[0]);
  EXPECT_EQ(3, bindings()->errors_line_number[0]);
}

// Execute a PAC script which throws an exception when first accessing
// FindProxyForURL
TEST_F(ProxyResolverV8Test, ExceptionAccessingFindProxyForURLDuringInit) {
  EXPECT_EQ(net::ERR_PAC_SCRIPT_FAILED,
            CreateResolver("exception_findproxyforurl_during_init.js"));

  ASSERT_EQ(2U, bindings()->errors.size());
  EXPECT_EQ("Uncaught crash!", bindings()->errors[0]);
  EXPECT_EQ(9, bindings()->errors_line_number[0]);
  EXPECT_EQ("Accessing FindProxyForURL threw an exception.",
            bindings()->errors[1]);
  EXPECT_EQ(-1, bindings()->errors_line_number[1]);
}

// Execute a PAC script which throws an exception during the second access to
// FindProxyForURL
TEST_F(ProxyResolverV8Test, ExceptionAccessingFindProxyForURLDuringResolve) {
  ASSERT_THAT(CreateResolver("exception_findproxyforurl_during_resolve.js"),
              IsOk());

  net::ProxyInfo proxy_info;
  int result = resolver().GetProxyForURL(kQueryUrl, &proxy_info, bindings());

  EXPECT_THAT(result, IsError(net::ERR_PAC_SCRIPT_FAILED));

  ASSERT_EQ(2U, bindings()->errors.size());
  EXPECT_EQ("Uncaught crash!", bindings()->errors[0]);
  EXPECT_EQ(17, bindings()->errors_line_number[0]);
  EXPECT_EQ("Accessing FindProxyForURL threw an exception.",
            bindings()->errors[1]);
  EXPECT_EQ(-1, bindings()->errors_line_number[1]);
}

TEST_F(ProxyResolverV8Test, ReturnUnicode) {
  ASSERT_THAT(CreateResolver("return_unicode.js"), IsOk());

  net::ProxyInfo proxy_info;
  int result = resolver().GetProxyForURL(kQueryUrl, &proxy_info, bindings());

  // The result from this resolve was unparseable, because it
  // wasn't ASCII.
  EXPECT_THAT(result, IsError(net::ERR_PAC_SCRIPT_FAILED));
}

// Test the PAC library functions that we expose in the JS environment.
TEST_F(ProxyResolverV8Test, JavascriptLibrary) {
  ASSERT_THAT(CreateResolver("pac_library_unittest.js"), IsOk());

  net::ProxyInfo proxy_info;
  int result = resolver().GetProxyForURL(kQueryUrl, &proxy_info, bindings());

  // If the javascript side of this unit-test fails, it will throw a javascript
  // exception. Otherwise it will return "PROXY success:80".
  EXPECT_THAT(bindings()->alerts, IsEmpty());
  EXPECT_THAT(bindings()->errors, IsEmpty());

  ASSERT_THAT(result, IsOk());
  EXPECT_EQ("success:80", proxy_info.proxy_server().ToURI());
}

// Test marshalling/un-marshalling of values between C++/V8.
TEST_F(ProxyResolverV8Test, V8Bindings) {
  ASSERT_THAT(CreateResolver("bindings.js"), IsOk());
  bindings()->dns_resolve_result = "127.0.0.1";

  net::ProxyInfo proxy_info;
  int result = resolver().GetProxyForURL(kQueryUrl, &proxy_info, bindings());

  EXPECT_THAT(result, IsOk());
  EXPECT_TRUE(proxy_info.is_direct());

  EXPECT_EQ(0U, bindings()->errors.size());

  // Alert was called 5 times.
  ASSERT_EQ(5U, bindings()->alerts.size());
  EXPECT_EQ("undefined", bindings()->alerts[0]);
  EXPECT_EQ("null", bindings()->alerts[1]);
  EXPECT_EQ("undefined", bindings()->alerts[2]);
  EXPECT_EQ("[object Object]", bindings()->alerts[3]);
  EXPECT_EQ("exception from calling toString()", bindings()->alerts[4]);

  // DnsResolve was called 8 times, however only 2 of those were string
  // parameters. (so 6 of them failed immediately).
  ASSERT_EQ(2U, bindings()->dns_resolves.size());
  EXPECT_EQ("", bindings()->dns_resolves[0]);
  EXPECT_EQ("arg1", bindings()->dns_resolves[1]);

  // MyIpAddress was called two times.
  EXPECT_EQ(2, bindings()->my_ip_address_count);

  // MyIpAddressEx was called once.
  EXPECT_EQ(1, bindings()->my_ip_address_ex_count);

  // DnsResolveEx was called 2 times.
  ASSERT_EQ(2U, bindings()->dns_resolves_ex.size());
  EXPECT_EQ("is_resolvable", bindings()->dns_resolves_ex[0]);
  EXPECT_EQ("foobar", bindings()->dns_resolves_ex[1]);
}

// Test calling a binding (myIpAddress()) from the script's global scope.
// http://crbug.com/40026
TEST_F(ProxyResolverV8Test, BindingCalledDuringInitialization) {
  ASSERT_THAT(CreateResolver("binding_from_global.js"), IsOk());

  // myIpAddress() got called during initialization of the script.
  EXPECT_EQ(1, bindings()->my_ip_address_count);

  net::ProxyInfo proxy_info;
  int result = resolver().GetProxyForURL(kQueryUrl, &proxy_info, bindings());

  EXPECT_THAT(result, IsOk());
  EXPECT_FALSE(proxy_info.is_direct());
  EXPECT_EQ("127.0.0.1:80", proxy_info.proxy_server().ToURI());

  // Check that no other bindings were called.
  EXPECT_EQ(0U, bindings()->errors.size());
  ASSERT_EQ(0U, bindings()->alerts.size());
  ASSERT_EQ(0U, bindings()->dns_resolves.size());
  EXPECT_EQ(0, bindings()->my_ip_address_ex_count);
  ASSERT_EQ(0U, bindings()->dns_resolves_ex.size());
}

// Try loading a PAC script that ends with a comment and has no terminal
// newline. This should not cause problems with the PAC utility functions
// that we add to the script's environment.
// http://crbug.com/22864
TEST_F(ProxyResolverV8Test, EndsWithCommentNoNewline) {
  ASSERT_THAT(CreateResolver("ends_with_comment.js"), IsOk());

  net::ProxyInfo proxy_info;
  int result = resolver().GetProxyForURL(kQueryUrl, &proxy_info, bindings());

  EXPECT_THAT(result, IsOk());
  EXPECT_FALSE(proxy_info.is_direct());
  EXPECT_EQ("success:80", proxy_info.proxy_server().ToURI());
}

// Try loading a PAC script that ends with a statement and has no terminal
// newline. This should not cause problems with the PAC utility functions
// that we add to the script's environment.
// http://crbug.com/22864
TEST_F(ProxyResolverV8Test, EndsWithStatementNoNewline) {
  ASSERT_THAT(CreateResolver("ends_with_statement_no_semicolon.js"), IsOk());

  net::ProxyInfo proxy_info;
  int result = resolver().GetProxyForURL(kQueryUrl, &proxy_info, bindings());

  EXPECT_THAT(result, IsOk());
  EXPECT_FALSE(proxy_info.is_direct());
  EXPECT_EQ("success:3", proxy_info.proxy_server().ToURI());
}

// Test the return values from myIpAddress(), myIpAddressEx(), dnsResolve(),
// dnsResolveEx(), isResolvable(), isResolvableEx(), when the the binding
// returns empty string (failure). This simulates the return values from
// those functions when the underlying DNS resolution fails.
TEST_F(ProxyResolverV8Test, DNSResolutionFailure) {
  ASSERT_THAT(CreateResolver("dns_fail.js"), IsOk());

  net::ProxyInfo proxy_info;
  int result = resolver().GetProxyForURL(kQueryUrl, &proxy_info, bindings());

  EXPECT_THAT(result, IsOk());
  EXPECT_FALSE(proxy_info.is_direct());
  EXPECT_EQ("success:80", proxy_info.proxy_server().ToURI());
}

TEST_F(ProxyResolverV8Test, DNSResolutionOfInternationDomainName) {
  ASSERT_THAT(CreateResolver("international_domain_names.js"), IsOk());

  // Execute FindProxyForURL().
  net::ProxyInfo proxy_info;
  int result = resolver().GetProxyForURL(kQueryUrl, &proxy_info, bindings());

  EXPECT_THAT(result, IsOk());
  EXPECT_TRUE(proxy_info.is_direct());

  // Check that the international domain name was converted to punycode
  // before passing it onto the bindings layer.
  ASSERT_EQ(1u, bindings()->dns_resolves.size());
  EXPECT_EQ("xn--bcher-kva.ch", bindings()->dns_resolves[0]);

  ASSERT_EQ(1u, bindings()->dns_resolves_ex.size());
  EXPECT_EQ("xn--bcher-kva.ch", bindings()->dns_resolves_ex[0]);
}

// Test that when resolving a URL which contains an IPv6 string literal, the
// brackets are removed from the host before passing it down to the PAC script.
// If we don't do this, then subsequent calls to dnsResolveEx(host) will be
// doomed to fail since it won't correspond with a valid name.
TEST_F(ProxyResolverV8Test, IPv6HostnamesNotBracketed) {
  ASSERT_THAT(CreateResolver("resolve_host.js"), IsOk());

  net::ProxyInfo proxy_info;
  int result = resolver().GetProxyForURL(
      GURL("http://[abcd::efff]:99/watsupdawg"), &proxy_info, bindings());

  EXPECT_THAT(result, IsOk());
  EXPECT_TRUE(proxy_info.is_direct());

  // We called dnsResolveEx() exactly once, by passing through the "host"
  // argument to FindProxyForURL(). The brackets should have been stripped.
  ASSERT_EQ(1U, bindings()->dns_resolves_ex.size());
  EXPECT_EQ("abcd::efff", bindings()->dns_resolves_ex[0]);
}

// Test that terminating a script within DnsResolve() leads to eventual
// termination of the script. Also test that repeatedly calling terminate is
// safe, and running the script again after termination still works.
TEST_F(ProxyResolverV8Test, Terminate) {
  ASSERT_THAT(CreateResolver("terminate.js"), IsOk());

  // Terminate script execution upon reaching dnsResolve(). Note that
  // termination may not take effect right away (so the subsequent dnsResolve()
  // and alert() may be run).
  bindings()->should_terminate = true;

  net::ProxyInfo proxy_info;
  int result =
      resolver().GetProxyForURL(GURL("http://hang/"), &proxy_info, bindings());

  // The script execution was terminated.
  EXPECT_THAT(result, IsError(net::ERR_PAC_SCRIPT_FAILED));

  EXPECT_EQ(1U, bindings()->dns_resolves.size());
  EXPECT_GE(2U, bindings()->dns_resolves_ex.size());
  EXPECT_GE(1U, bindings()->alerts.size());

  EXPECT_EQ(1U, bindings()->errors.size());

  // Termination shows up as an uncaught exception without any message.
  EXPECT_EQ("", bindings()->errors[0]);

  bindings()->errors.clear();

  // Try running the script again, this time with a different input which won't
  // cause a termination+hang.
  result = resolver().GetProxyForURL(GURL("http://kittens/"), &proxy_info,
                                     bindings());

  EXPECT_THAT(result, IsOk());
  EXPECT_EQ(0u, bindings()->errors.size());
  EXPECT_EQ("kittens:88", proxy_info.proxy_server().ToURI());
}

}  // namespace
}  // namespace proxy_resolver
