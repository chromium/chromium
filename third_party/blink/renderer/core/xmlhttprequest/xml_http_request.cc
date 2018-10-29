/*
 *  Copyright (C) 2004, 2006, 2008 Apple Inc. All rights reserved.
 *  Copyright (C) 2005-2007 Alexey Proskuryakov <ap@webkit.org>
 *  Copyright (C) 2007, 2008 Julien Chaffraix <jchaffraix@webkit.org>
 *  Copyright (C) 2008, 2011 Google Inc. All rights reserved.
 *  Copyright (C) 2012 Intel Corporation
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor,
 *  Boston, MA 02110-1301 USA
 */

#include "third_party/blink/renderer/core/xmlhttprequest/xml_http_request.h"

#include <memory>

#include "base/auto_reset.h"
#include "third_party/blink/public/common/blob/blob_utils.h"
#include "third_party/blink/public/mojom/feature_policy/feature_policy.mojom-blink.h"
#include "third_party/blink/public/platform/web_cors.h"
#include "third_party/blink/public/platform/web_url_request.h"
#include "third_party/blink/renderer/bindings/core/v8/array_buffer_or_array_buffer_view_or_blob_or_document_or_string_or_form_data_or_url_search_params.h"
#include "third_party/blink/renderer/bindings/core/v8/array_buffer_or_array_buffer_view_or_blob_or_usv_string.h"
#include "third_party/blink/renderer/core/dom/document_init.h"
#include "third_party/blink/renderer/core/dom/document_parser.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/dom/dom_implementation.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/dom/xml_document.h"
#include "third_party/blink/renderer/core/editing/serializers/serialization.h"
#include "third_party/blink/renderer/core/events/progress_event.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/fileapi/blob.h"
#include "third_party/blink/renderer/core/fileapi/file.h"
#include "third_party/blink/renderer/core/fileapi/file_reader_loader.h"
#include "third_party/blink/renderer/core/fileapi/file_reader_loader_client.h"
#include "third_party/blink/renderer/core/fileapi/public_url_manager.h"
#include "third_party/blink/renderer/core/frame/csp/content_security_policy.h"
#include "third_party/blink/renderer/core/frame/deprecation.h"
#include "third_party/blink/renderer/core/frame/frame.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/frame/use_counter.h"
#include "third_party/blink/renderer/core/html/forms/form_data.h"
#include "third_party/blink/renderer/core/html/html_document.h"
#include "third_party/blink/renderer/core/html/parser/text_resource_decoder.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/core/inspector/inspector_trace_events.h"
#include "third_party/blink/renderer/core/loader/threadable_loader.h"
#include "third_party/blink/renderer/core/page/chrome_client.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/probe/core_probes.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_array_buffer.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_array_buffer_view.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_typed_array.h"
#include "third_party/blink/renderer/core/url/url_search_params.h"
#include "third_party/blink/renderer/core/xmlhttprequest/xml_http_request_upload.h"
#include "third_party/blink/renderer/platform/bindings/dom_wrapper_world.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/blob/blob_data.h"
#include "third_party/blink/renderer/platform/exported/wrapped_resource_response.h"
#include "third_party/blink/renderer/platform/file_metadata.h"
#include "third_party/blink/renderer/platform/histogram.h"
#include "third_party/blink/renderer/platform/loader/cors/cors.h"
#include "third_party/blink/renderer/platform/loader/fetch/fetch_initiator_type_names.h"
#include "third_party/blink/renderer/platform/loader/fetch/fetch_utils.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_error.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_loader_options.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_request.h"
#include "third_party/blink/renderer/platform/loader/fetch/text_resource_decoder_options.h"
#include "third_party/blink/renderer/platform/network/http_names.h"
#include "third_party/blink/renderer/platform/network/http_parsers.h"
#include "third_party/blink/renderer/platform/network/network_log.h"
#include "third_party/blink/renderer/platform/network/parsed_content_type.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/shared_buffer.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"
#include "third_party/blink/renderer/platform/weborigin/security_policy.h"
#include "third_party/blink/renderer/platform/wtf/assertions.h"
#include "third_party/blink/renderer/platform/wtf/std_lib_extras.h"
#include "third_party/blink/renderer/platform/wtf/text/cstring.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace blink {

namespace {

// This class protects the wrapper of the associated XMLHttpRequest object
// via hasPendingActivity method which returns true if
// m_eventDispatchRecursionLevel is positive.
class ScopedEventDispatchProtect final {
 public:
  explicit ScopedEventDispatchProtect(int* level) : level_(level) { ++*level_; }
  ~ScopedEventDispatchProtect() {
    DCHECK_GT(*level_, 0);
    --*level_;
  }

 private:
  int* const level_;
};

// These methods were placed in HTTPParsers.h. Since these methods don't
// perform ABNF validation but loosely look for the part that is likely to be
// indicating the charset parameter, new code should use
// HttpUtil::ParseContentType() than these. To discourage use of these methods,
// moved from HTTPParser.h to the only user XMLHttpRequest.cpp.
//
// TODO(tyoshino): Switch XHR to use HttpUtil. See crbug.com/743311.
void FindCharsetInMediaType(const String& media_type,
                            unsigned& charset_pos,
                            unsigned& charset_len) {
  charset_len = 0;

  size_t pos = charset_pos;
  unsigned length = media_type.length();

  while (pos < length) {
    pos = media_type.FindIgnoringASCIICase("charset", pos);

    if (pos == kNotFound)
      return;

    // Give up if we find "charset" at the head.
    if (!pos)
      return;

    // Now check that "charset" is not a substring of some longer word.
    if (media_type[pos - 1] > ' ' && media_type[pos - 1] != ';') {
      pos += 7;
      continue;
    }

    pos += 7;

    while (pos < length && media_type[pos] <= ' ')
      ++pos;

    // Treat this as a charset parameter.
    if (media_type[pos++] == '=')
      break;
  }

  while (pos < length && (media_type[pos] <= ' ' || media_type[pos] == '"' ||
                          media_type[pos] == '\''))
    ++pos;

  charset_pos = pos;

  // we don't handle spaces within quoted parameter values, because charset
  // names cannot have any
  while (pos < length && media_type[pos] > ' ' && media_type[pos] != '"' &&
         media_type[pos] != '\'' && media_type[pos] != ';')
    ++pos;

  charset_len = pos - charset_pos;
}
String ExtractCharsetFromMediaType(const String& media_type) {
  unsigned pos = 0;
  unsigned len = 0;
  FindCharsetInMediaType(media_type, pos, len);
  return media_type.Substring(pos, len);
}

void ReplaceCharsetInMediaType(String& media_type,
                               const String& charset_value) {
  unsigned pos = 0;

  while (true) {
    unsigned len = 0;
    FindCharsetInMediaType(media_type, pos, len);
    if (!len)
      return;
    media_type.replace(pos, len, charset_value);
    pos += charset_value.length();
  }
}

void LogConsoleError(ExecutionContext* context, const String& message) {
  if (!context)
    return;
  // FIXME: It's not good to report the bad usage without indicating what source
  // line it came from.  We should pass additional parameters so we can tell the
  // console where the mistake occurred.
  ConsoleMessage* console_message =
      ConsoleMessage::Create(kJSMessageSource, kErrorMessageLevel, message);
  context->AddConsoleMessage(console_message);
}

bool ValidateOpenArguments(const AtomicString& method,
                           const KURL& url,
                           ExceptionState& exception_state) {
  if (!IsValidHTTPToken(method)) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kSyntaxError,
        "'" + method + "' is not a valid HTTP method.");
    return false;
  }

  if (FetchUtils::IsForbiddenMethod(method)) {
    exception_state.ThrowSecurityError("'" + method +
                                       "' HTTP method is unsupported.");
    return false;
  }

  if (!url.IsValid()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kSyntaxError,
                                      "Invalid URL");
    return false;
  }

  return true;
}

}  // namespace

class XMLHttpRequest::BlobLoader final
    : public GarbageCollectedFinalized<XMLHttpRequest::BlobLoader>,
      public FileReaderLoaderClient {
 public:
  static BlobLoader* Create(XMLHttpRequest* xhr,
                            scoped_refptr<BlobDataHandle> handle) {
    return new BlobLoader(xhr, std::move(handle));
  }

  // FileReaderLoaderClient functions.
  void DidStartLoading() override {}
  void DidReceiveDataForClient(const char* data, unsigned length) override {
    DCHECK_LE(length, static_cast<unsigned>(INT_MAX));
    xhr_->DidReceiveData(data, length);
  }
  void DidFinishLoading() override { xhr_->DidFinishLoadingFromBlob(); }
  void DidFail(FileError::ErrorCode error) override {
    xhr_->DidFailLoadingFromBlob();
  }

  void Cancel() { loader_->Cancel(); }

  void Trace(blink::Visitor* visitor) { visitor->Trace(xhr_); }

 private:
  BlobLoader(XMLHttpRequest* xhr, scoped_refptr<BlobDataHandle> handle)
      : xhr_(xhr),
        loader_(
            FileReaderLoader::Create(FileReaderLoader::kReadByClient, this)) {
    loader_->Start(std::move(handle));
  }

  Member<XMLHttpRequest> xhr_;
  std::unique_ptr<FileReaderLoader> loader_;
};

XMLHttpRequest* XMLHttpRequest::Create(ScriptState* script_state) {
  ExecutionContext* context = ExecutionContext::From(script_state);
  DOMWrapperWorld& world = script_state->World();
  v8::Isolate* isolate = script_state->GetIsolate();

  XMLHttpRequest* xml_http_request =
      world.IsIsolatedWorld()
          ? new XMLHttpRequest(context, isolate, true,
                               world.IsolatedWorldSecurityOrigin())
          : new XMLHttpRequest(context, isolate, false, nullptr);
  xml_http_request->PauseIfNeeded();
  return xml_http_request;
}

XMLHttpRequest* XMLHttpRequest::Create(ExecutionContext* context) {
  v8::Isolate* isolate = ToIsolate(context);
  CHECK(isolate);

  XMLHttpRequest* xml_http_request =
      new XMLHttpRequest(context, isolate, false, nullptr);
  xml_http_request->PauseIfNeeded();
  return xml_http_request;
}

XMLHttpRequest::XMLHttpRequest(
    ExecutionContext* context,
    v8::Isolate* isolate,
    bool is_isolated_world,
    scoped_refptr<SecurityOrigin> isolated_world_security_origin)
    : PausableObject(context),
      progress_event_throttle_(
          XMLHttpRequestProgressEventThrottle::Create(this)),
      isolate_(isolate),
      is_isolated_world_(is_isolated_world),
      isolated_world_security_origin_(
          std::move(isolated_world_security_origin)) {}

XMLHttpRequest::~XMLHttpRequest() {
  binary_response_builder_ = nullptr;
  length_downloaded_to_blob_ = 0;
  ReportMemoryUsageToV8();
}

Document* XMLHttpRequest::GetDocument() const {
  return To<Document>(GetExecutionContext());
}

const SecurityOrigin* XMLHttpRequest::GetSecurityOrigin() const {
  return isolated_world_security_origin_
             ? isolated_world_security_origin_.get()
             : GetExecutionContext()->GetSecurityOrigin();
}

SecurityOrigin* XMLHttpRequest::GetMutableSecurityOrigin() {
  return isolated_world_security_origin_
             ? isolated_world_security_origin_.get()
             : GetExecutionContext()->GetMutableSecurityOrigin();
}

XMLHttpRequest::State XMLHttpRequest::readyState() const {
  return state_;
}

v8::Local<v8::String> XMLHttpRequest::responseText(
    ExceptionState& exception_state) {
  if (response_type_code_ != kResponseTypeDefault &&
      response_type_code_ != kResponseTypeText) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "The value is only accessible if the "
                                      "object's 'responseType' is '' or 'text' "
                                      "(was '" +
                                          responseType() + "').");
    return v8::Local<v8::String>();
  }
  if (error_ || (state_ != kLoading && state_ != kDone))
    return v8::Local<v8::String>();
  return response_text_.V8Value(isolate_);
}

v8::Local<v8::String> XMLHttpRequest::ResponseJSONSource() {
  DCHECK_EQ(response_type_code_, kResponseTypeJSON);

  if (error_ || state_ != kDone)
    return v8::Local<v8::String>();
  return response_text_.V8Value(isolate_);
}

void XMLHttpRequest::InitResponseDocument() {
  // The W3C spec requires the final MIME type to be some valid XML type, or
  // text/html.  If it is text/html, then the responseType of "document" must
  // have been supplied explicitly.
  bool is_html = ResponseIsHTML();
  if ((response_.IsHTTP() && !ResponseIsXML() && !is_html) ||
      (is_html && response_type_code_ == kResponseTypeDefault) ||
      !GetExecutionContext() || GetExecutionContext()->IsWorkerGlobalScope()) {
    response_document_ = nullptr;
    return;
  }

  DocumentInit init = DocumentInit::Create()
                          .WithContextDocument(GetDocument()->ContextDocument())
                          .WithURL(response_.Url());
  if (is_html)
    response_document_ = HTMLDocument::Create(init);
  else
    response_document_ = XMLDocument::Create(init);

  // FIXME: Set Last-Modified.
  response_document_->SetSecurityOrigin(GetMutableSecurityOrigin());
  response_document_->SetContextFeatures(GetDocument()->GetContextFeatures());
  response_document_->SetMimeType(FinalResponseMIMETypeWithFallback());
}

Document* XMLHttpRequest::responseXML(ExceptionState& exception_state) {
  if (response_type_code_ != kResponseTypeDefault &&
      response_type_code_ != kResponseTypeDocument) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "The value is only accessible if the "
                                      "object's 'responseType' is '' or "
                                      "'document' (was '" +
                                          responseType() + "').");
    return nullptr;
  }

  if (error_ || state_ != kDone)
    return nullptr;

  if (!parsed_response_) {
    InitResponseDocument();
    if (!response_document_)
      return nullptr;

    response_document_->SetContent(response_text_.Flatten(isolate_));
    if (!response_document_->WellFormed())
      response_document_ = nullptr;

    parsed_response_ = true;
  }

  return response_document_;
}

Blob* XMLHttpRequest::ResponseBlob() {
  DCHECK_EQ(response_type_code_, kResponseTypeBlob);

  // We always return null before kDone.
  if (error_ || state_ != kDone)
    return nullptr;

  if (!response_blob_) {
    std::unique_ptr<BlobData> blob_data = BlobData::Create();
    blob_data->SetContentType(FinalResponseMIMETypeWithFallback().LowerASCII());
    size_t size = 0;
    if (binary_response_builder_ && binary_response_builder_->size()) {
      for (const auto& span : *binary_response_builder_)
        blob_data->AppendBytes(span.data(), span.size());
      size = binary_response_builder_->size();
      binary_response_builder_ = nullptr;
      ReportMemoryUsageToV8();
    }
    response_blob_ =
        Blob::Create(BlobDataHandle::Create(std::move(blob_data), size));
  }

  return response_blob_;
}

DOMArrayBuffer* XMLHttpRequest::ResponseArrayBuffer() {
  DCHECK_EQ(response_type_code_, kResponseTypeArrayBuffer);

  if (error_ || state_ != kDone)
    return nullptr;

  if (!response_array_buffer_ && !response_array_buffer_failure_) {
    if (binary_response_builder_ && binary_response_builder_->size()) {
      DOMArrayBuffer* buffer = DOMArrayBuffer::CreateUninitializedOrNull(
          binary_response_builder_->size(), 1);
      if (buffer) {
        bool result = binary_response_builder_->GetBytes(
            buffer->Data(), static_cast<size_t>(buffer->ByteLength()));
        DCHECK(result);
        response_array_buffer_ = buffer;
      }
      // https://xhr.spec.whatwg.org/#arraybuffer-response allows clearing
      // of the 'received bytes' payload when the response buffer allocation
      // fails.
      binary_response_builder_ = nullptr;
      ReportMemoryUsageToV8();
      // Mark allocation as failed; subsequent calls to the accessor must
      // continue to report |null|.
      //
      response_array_buffer_failure_ = !buffer;
    } else {
      response_array_buffer_ = DOMArrayBuffer::Create(nullptr, 0);
    }
  }

  return response_array_buffer_;
}

void XMLHttpRequest::setTimeout(unsigned timeout,
                                ExceptionState& exception_state) {
  // FIXME: Need to trigger or update the timeout Timer here, if needed.
  // http://webkit.org/b/98156
  // XHR2 spec, 4.7.3. "This implies that the timeout attribute can be set while
  // fetching is in progress. If that occurs it will still be measured relative
  // to the start of fetching."
  if (GetExecutionContext() && GetExecutionContext()->IsDocument() && !async_) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidAccessError,
                                      "Timeouts cannot be set for synchronous "
                                      "requests made from a document.");
    return;
  }

  timeout_ = TimeDelta::FromMilliseconds(timeout);

  // From http://www.w3.org/TR/XMLHttpRequest/#the-timeout-attribute:
  // Note: This implies that the timeout attribute can be set while fetching is
  // in progress. If that occurs it will still be measured relative to the start
  // of fetching.
  //
  // The timeout may be overridden after send.
  if (loader_)
    loader_->SetTimeout(timeout_);
}

void XMLHttpRequest::setResponseType(const String& response_type,
                                     ExceptionState& exception_state) {
  if (state_ >= kLoading) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "The response type cannot be set if the "
                                      "object's state is LOADING or DONE.");
    return;
  }

  // Newer functionality is not available to synchronous requests in window
  // contexts, as a spec-mandated attempt to discourage synchronous XHR use.
  // responseType is one such piece of functionality.
  if (GetExecutionContext() && GetExecutionContext()->IsDocument() && !async_) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidAccessError,
                                      "The response type cannot be changed for "
                                      "synchronous requests made from a "
                                      "document.");
    return;
  }

  if (response_type == "") {
    response_type_code_ = kResponseTypeDefault;
  } else if (response_type == "text") {
    response_type_code_ = kResponseTypeText;
  } else if (response_type == "json") {
    response_type_code_ = kResponseTypeJSON;
  } else if (response_type == "document") {
    response_type_code_ = kResponseTypeDocument;
  } else if (response_type == "blob") {
    response_type_code_ = kResponseTypeBlob;
  } else if (response_type == "arraybuffer") {
    response_type_code_ = kResponseTypeArrayBuffer;
  } else {
    NOTREACHED();
  }
}

String XMLHttpRequest::responseType() {
  switch (response_type_code_) {
    case kResponseTypeDefault:
      return "";
    case kResponseTypeText:
      return "text";
    case kResponseTypeJSON:
      return "json";
    case kResponseTypeDocument:
      return "document";
    case kResponseTypeBlob:
      return "blob";
    case kResponseTypeArrayBuffer:
      return "arraybuffer";
  }
  return "";
}

String XMLHttpRequest::responseURL() {
  KURL response_url(response_.Url());
  if (!response_url.IsNull())
    response_url.RemoveFragmentIdentifier();
  return response_url.GetString();
}

XMLHttpRequestUpload* XMLHttpRequest::upload() {
  if (!upload_)
    upload_ = XMLHttpRequestUpload::Create(this);
  return upload_;
}

void XMLHttpRequest::TrackProgress(long long length) {
  received_length_ += length;

  ChangeState(kLoading);
  if (async_) {
    // readyStateChange event is fired as well.
    DispatchProgressEventFromSnapshot(EventTypeNames::progress);
  }
}

void XMLHttpRequest::ChangeState(State new_state) {
  if (state_ != new_state) {
    state_ = new_state;
    DispatchReadyStateChangeEvent();
  }
}

void XMLHttpRequest::DispatchReadyStateChangeEvent() {
  if (!GetExecutionContext())
    return;

  ScopedEventDispatchProtect protect(&event_dispatch_recursion_level_);
  if (async_ || (state_ <= kOpened || state_ == kDone)) {
    TRACE_EVENT1(
        "devtools.timeline", "XHRReadyStateChange", "data",
        InspectorXhrReadyStateChangeEvent::Data(GetExecutionContext(), this));
    XMLHttpRequestProgressEventThrottle::DeferredEventAction action =
        XMLHttpRequestProgressEventThrottle::kIgnore;
    if (state_ == kDone) {
      if (error_)
        action = XMLHttpRequestProgressEventThrottle::kClear;
      else
        action = XMLHttpRequestProgressEventThrottle::kFlush;
    }
    progress_event_throttle_->DispatchReadyStateChangeEvent(
        Event::Create(EventTypeNames::readystatechange), action);
  }

  if (state_ == kDone && !error_) {
    TRACE_EVENT1("devtools.timeline", "XHRLoad", "data",
                 InspectorXhrLoadEvent::Data(GetExecutionContext(), this));
    DispatchProgressEventFromSnapshot(EventTypeNames::load);
    DispatchProgressEventFromSnapshot(EventTypeNames::loadend);
  }
}

void XMLHttpRequest::setWithCredentials(bool value,
                                        ExceptionState& exception_state) {
  if (state_ > kOpened || send_flag_) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidStateError,
        "The value may only be set if the object's state is UNSENT or OPENED.");
    return;
  }

  with_credentials_ = value;
}

void XMLHttpRequest::open(const AtomicString& method,
                          const String& url_string,
                          ExceptionState& exception_state) {
  if (!GetExecutionContext())
    return;

  KURL url(GetExecutionContext()->CompleteURL(url_string));
  if (!ValidateOpenArguments(method, url, exception_state))
    return;

  open(method, url, true, exception_state);
}

void XMLHttpRequest::open(const AtomicString& method,
                          const String& url_string,
                          bool async,
                          const String& username,
                          const String& password,
                          ExceptionState& exception_state) {
  if (!GetExecutionContext())
    return;

  KURL url(GetExecutionContext()->CompleteURL(url_string));
  if (!ValidateOpenArguments(method, url, exception_state))
    return;

  if (!username.IsNull())
    url.SetUser(username);
  if (!password.IsNull())
    url.SetPass(password);

  open(method, url, async, exception_state);
}

void XMLHttpRequest::open(const AtomicString& method,
                          const KURL& url,
                          bool async,
                          ExceptionState& exception_state) {
  NETWORK_DVLOG(1) << this << " open(" << method << ", " << url.ElidedString()
                   << ", " << async << ")";

  DCHECK(ValidateOpenArguments(method, url, exception_state));

  if (!InternalAbort())
    return;

  State previous_state = state_;
  state_ = kUnsent;
  error_ = false;
  upload_complete_ = false;

  if (!async && GetExecutionContext()->IsDocument()) {
    if (GetDocument()->GetSettings() &&
        !GetDocument()->GetSettings()->GetSyncXHRInDocumentsEnabled()) {
      exception_state.ThrowDOMException(
          DOMExceptionCode::kInvalidAccessError,
          "Synchronous requests are disabled for this page.");
      return;
    }

    // Newer functionality is not available to synchronous requests in window
    // contexts, as a spec-mandated attempt to discourage synchronous XHR use.
    // responseType is one such piece of functionality.
    if (response_type_code_ != kResponseTypeDefault) {
      exception_state.ThrowDOMException(
          DOMExceptionCode::kInvalidAccessError,
          "Synchronous requests from a document must not set a response type.");
      return;
    }

    // Similarly, timeouts are disabled for synchronous requests as well.
    if (!timeout_.is_zero()) {
      exception_state.ThrowDOMException(
          DOMExceptionCode::kInvalidAccessError,
          "Synchronous requests must not set a timeout.");
      return;
    }

    // Here we just warn that firing sync XHR's may affect responsiveness.
    // Eventually sync xhr will be deprecated and an "InvalidAccessError"
    // exception thrown.
    // Refer : https://xhr.spec.whatwg.org/#sync-warning
    // Use count for XHR synchronous requests on main thread only.
    if (!GetDocument()->ProcessingBeforeUnload()) {
      Deprecation::CountDeprecation(
          GetExecutionContext(),
          WebFeature::kXMLHttpRequestSynchronousInNonWorkerOutsideBeforeUnload);
    }
  }

  method_ = FetchUtils::NormalizeMethod(method);

  url_ = url;

  if (url_.ProtocolIs("blob") && BlobUtils::MojoBlobURLsEnabled()) {
    GetExecutionContext()->GetPublicURLManager().Resolve(
        url_, MakeRequest(&blob_url_loader_factory_));
  }

  async_ = async;

  DCHECK(!loader_);
  send_flag_ = false;

  // Check previous state to avoid dispatching readyState event
  // when calling open several times in a row.
  if (previous_state != kOpened)
    ChangeState(kOpened);
  else
    state_ = kOpened;
}

bool XMLHttpRequest::InitSend(ExceptionState& exception_state) {
  // We need to check ContextDestroyed because it is possible to create a
  // XMLHttpRequest with already detached document.
  // TODO(yhirano): Fix this.
  if (!GetExecutionContext() || GetExecutionContext()->IsContextDestroyed()) {
    HandleNetworkError();
    ThrowForLoadFailureIfNeeded(exception_state,
                                "Document is already detached.");
    return false;
  }

  if (state_ != kOpened || send_flag_) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "The object's state must be OPENED.");
    return false;
  }

  if (!async_) {
    if (GetExecutionContext()->IsDocument() &&
        !GetDocument()->IsFeatureEnabled(
            mojom::FeaturePolicyFeature::kSyncXHR,
            ReportOptions::kReportOnFailure,
            "Synchronous requests are disabled by Feature Policy.")) {
      HandleNetworkError();
      ThrowForLoadFailureIfNeeded(exception_state, String());
      return false;
    }
    v8::Isolate* isolate = v8::Isolate::GetCurrent();
    if (isolate && v8::MicrotasksScope::IsRunningMicrotasks(isolate)) {
      UseCounter::Count(GetExecutionContext(),
                        WebFeature::kDuring_Microtask_SyncXHR);
    }
  }

  error_ = false;
  return true;
}

void XMLHttpRequest::send(
    const ArrayBufferOrArrayBufferViewOrBlobOrDocumentOrStringOrFormDataOrURLSearchParams&
        body,
    ExceptionState& exception_state) {
  probe::willSendXMLHttpOrFetchNetworkRequest(GetExecutionContext(), Url());

  if (body.IsNull()) {
    send(String(), exception_state);
    return;
  }

  if (body.IsArrayBuffer()) {
    send(body.GetAsArrayBuffer(), exception_state);
    return;
  }

  if (body.IsArrayBufferView()) {
    send(body.GetAsArrayBufferView().View(), exception_state);
    return;
  }

  if (body.IsBlob()) {
    send(body.GetAsBlob(), exception_state);
    return;
  }

  if (body.IsDocument()) {
    send(body.GetAsDocument(), exception_state);
    return;
  }

  if (body.IsFormData()) {
    send(body.GetAsFormData(), exception_state);
    return;
  }

  if (body.IsURLSearchParams()) {
    send(body.GetAsURLSearchParams(), exception_state);
    return;
  }

  DCHECK(body.IsString());
  send(body.GetAsString(), exception_state);
}

bool XMLHttpRequest::AreMethodAndURLValidForSend() {
  return method_ != HTTPNames::GET && method_ != HTTPNames::HEAD &&
         url_.ProtocolIsInHTTPFamily();
}

void XMLHttpRequest::send(Document* document, ExceptionState& exception_state) {
  NETWORK_DVLOG(1) << this << " send() Document "
                   << static_cast<void*>(document);

  DCHECK(document);

  if (!InitSend(exception_state))
    return;

  scoped_refptr<EncodedFormData> http_body;

  if (AreMethodAndURLValidForSend()) {
    if (document->IsHTMLDocument())
      UpdateContentTypeAndCharset("text/html;charset=UTF-8", "UTF-8");
    else if (document->IsXMLDocument())
      UpdateContentTypeAndCharset("application/xml;charset=UTF-8", "UTF-8");

    String body = CreateMarkup(document);

    http_body = EncodedFormData::Create(
        UTF8Encoding().Encode(body, WTF::kNoUnencodables));
  }

  CreateRequest(std::move(http_body), exception_state);
}

void XMLHttpRequest::send(const String& body, ExceptionState& exception_state) {
  NETWORK_DVLOG(1) << this << " send() String " << body;

  if (!InitSend(exception_state))
    return;

  scoped_refptr<EncodedFormData> http_body;

  if (!body.IsNull() && AreMethodAndURLValidForSend()) {
    http_body = EncodedFormData::Create(
        UTF8Encoding().Encode(body, WTF::kNoUnencodables));
    UpdateContentTypeAndCharset("text/plain;charset=UTF-8", "UTF-8");
  }

  CreateRequest(std::move(http_body), exception_state);
}

void XMLHttpRequest::send(Blob* body, ExceptionState& exception_state) {
  NETWORK_DVLOG(1) << this << " send() Blob " << body->Uuid();

  if (!InitSend(exception_state))
    return;

  scoped_refptr<EncodedFormData> http_body;

  if (AreMethodAndURLValidForSend()) {
    if (!HasContentTypeRequestHeader()) {
      const String& blob_type = FetchUtils::NormalizeHeaderValue(body->type());
      if (!blob_type.IsEmpty() && ParsedContentType(blob_type).IsValid()) {
        SetRequestHeaderInternal(HTTPNames::Content_Type,
                                 AtomicString(blob_type));
      }
    }

    // FIXME: add support for uploading bundles.
    http_body = EncodedFormData::Create();
    if (body->HasBackingFile()) {
      File* file = ToFile(body);
      if (!file->GetPath().IsEmpty())
        http_body->AppendFile(file->GetPath());
      else
        NOTREACHED();
    } else {
      http_body->AppendBlob(body->Uuid(), body->GetBlobDataHandle());
    }
  }

  CreateRequest(std::move(http_body), exception_state);
}

void XMLHttpRequest::send(FormData* body, ExceptionState& exception_state) {
  NETWORK_DVLOG(1) << this << " send() FormData " << body;

  if (!InitSend(exception_state))
    return;

  scoped_refptr<EncodedFormData> http_body;

  if (AreMethodAndURLValidForSend()) {
    http_body = body->EncodeMultiPartFormData();

    // TODO (sof): override any author-provided charset= in the
    // content type value to UTF-8 ?
    if (!HasContentTypeRequestHeader()) {
      AtomicString content_type =
          AtomicString("multipart/form-data; boundary=") +
          FetchUtils::NormalizeHeaderValue(http_body->Boundary().data());
      SetRequestHeaderInternal(HTTPNames::Content_Type, content_type);
    }
  }

  CreateRequest(std::move(http_body), exception_state);
}

void XMLHttpRequest::send(URLSearchParams* body,
                          ExceptionState& exception_state) {
  NETWORK_DVLOG(1) << this << " send() URLSearchParams " << body;

  if (!InitSend(exception_state))
    return;

  scoped_refptr<EncodedFormData> http_body;

  if (AreMethodAndURLValidForSend()) {
    http_body = body->ToEncodedFormData();
    UpdateContentTypeAndCharset(
        "application/x-www-form-urlencoded;charset=UTF-8", "UTF-8");
  }

  CreateRequest(std::move(http_body), exception_state);
}

void XMLHttpRequest::send(DOMArrayBuffer* body,
                          ExceptionState& exception_state) {
  NETWORK_DVLOG(1) << this << " send() ArrayBuffer " << body;

  SendBytesData(body->Data(), body->ByteLength(), exception_state);
}

void XMLHttpRequest::send(DOMArrayBufferView* body,
                          ExceptionState& exception_state) {
  NETWORK_DVLOG(1) << this << " send() ArrayBufferView " << body;

  SendBytesData(body->BaseAddress(), body->byteLength(), exception_state);
}

void XMLHttpRequest::SendBytesData(const void* data,
                                   size_t length,
                                   ExceptionState& exception_state) {
  if (!InitSend(exception_state))
    return;

  scoped_refptr<EncodedFormData> http_body;

  if (AreMethodAndURLValidForSend()) {
    http_body = EncodedFormData::Create(data, length);
  }

  CreateRequest(std::move(http_body), exception_state);
}

void XMLHttpRequest::SendForInspectorXHRReplay(
    scoped_refptr<EncodedFormData> form_data,
    ExceptionState& exception_state) {
  CreateRequest(form_data ? form_data->DeepCopy() : nullptr, exception_state);
  if (exception_state.HadException()) {
    CHECK(IsDOMExceptionCode(exception_state.Code()));
    exception_code_ = exception_state.CodeAs<DOMExceptionCode>();
  }
}

void XMLHttpRequest::ThrowForLoadFailureIfNeeded(
    ExceptionState& exception_state,
    const String& reason) {
  if (error_ && exception_code_ == DOMExceptionCode::kNoError)
    exception_code_ = DOMExceptionCode::kNetworkError;

  if (exception_code_ == DOMExceptionCode::kNoError)
    return;

  String message = "Failed to load '" + url_.ElidedString() + "'";
  if (reason.IsNull()) {
    message.append('.');
  } else {
    message.append(": ");
    message.append(reason);
  }

  exception_state.ThrowDOMException(exception_code_, message);
}

void XMLHttpRequest::CreateRequest(scoped_refptr<EncodedFormData> http_body,
                                   ExceptionState& exception_state) {
  // Only GET request is supported for blob URL.
  if (url_.ProtocolIs("blob") && method_ != HTTPNames::GET) {
    HandleNetworkError();

    if (!async_) {
      ThrowForLoadFailureIfNeeded(
          exception_state,
          "'GET' is the only method allowed for 'blob:' URLs.");
    }
    return;
  }

  DCHECK(GetExecutionContext());
  ExecutionContext& execution_context = *GetExecutionContext();

  send_flag_ = true;
  // The presence of upload event listeners forces us to use preflighting
  // because POSTing to an URL that does not permit cross origin requests should
  // look exactly like POSTing to an URL that does not respond at all.
  // Also, only async requests support upload progress events.
  bool upload_events = false;
  if (async_) {
    probe::AsyncTaskScheduled(&execution_context, "XMLHttpRequest.send", this);
    DispatchProgressEvent(EventTypeNames::loadstart, 0, 0);
    // Event handler could have invalidated this send operation,
    // (re)setting the send flag and/or initiating another send
    // operation; leave quietly if so.
    if (!send_flag_ || loader_)
      return;
    if (http_body && upload_) {
      upload_events = upload_->HasEventListeners();
      upload_->DispatchEvent(
          *ProgressEvent::Create(EventTypeNames::loadstart, false, 0, 0));
      // See above.
      if (!send_flag_ || loader_)
        return;
    }
  }

  same_origin_request_ = GetSecurityOrigin()->CanRequest(url_);

  if (!same_origin_request_ && with_credentials_) {
    UseCounter::Count(&execution_context,
                      WebFeature::kXMLHttpRequestCrossOriginWithCredentials);
  }

  // We also remember whether upload events should be allowed for this request
  // in case the upload listeners are added after the request is started.
  upload_events_allowed_ =
      same_origin_request_ || upload_events ||
      !CORS::IsCORSSafelistedMethod(method_) ||
      !CORS::ContainsOnlyCORSSafelistedHeaders(request_headers_);

  ResourceRequest request(url_);
  request.SetRequestorOrigin(GetSecurityOrigin());
  request.SetHTTPMethod(method_);
  request.SetRequestContext(mojom::RequestContextType::XML_HTTP_REQUEST);
  request.SetFetchRequestMode(
      upload_events ? network::mojom::FetchRequestMode::kCORSWithForcedPreflight
                    : network::mojom::FetchRequestMode::kCORS);
  request.SetFetchCredentialsMode(
      with_credentials_ ? network::mojom::FetchCredentialsMode::kInclude
                        : network::mojom::FetchCredentialsMode::kSameOrigin);
  request.SetSkipServiceWorker(is_isolated_world_);
  request.SetExternalRequestStateFromRequestorAddressSpace(
      execution_context.GetSecurityContext().AddressSpace());

  probe::willLoadXHR(&execution_context, this, this, method_, url_, async_,
                     request_headers_, with_credentials_);

  if (http_body) {
    DCHECK_NE(method_, HTTPNames::GET);
    DCHECK_NE(method_, HTTPNames::HEAD);
    request.SetHTTPBody(std::move(http_body));
  }

  if (request_headers_.size() > 0)
    request.AddHTTPHeaderFields(request_headers_);

  ResourceLoaderOptions resource_loader_options;
  resource_loader_options.initiator_info.name =
      FetchInitiatorTypeNames::xmlhttprequest;
  if (blob_url_loader_factory_) {
    resource_loader_options.url_loader_factory = base::MakeRefCounted<
        base::RefCountedData<network::mojom::blink::URLLoaderFactoryPtr>>(
        std::move(blob_url_loader_factory_));
  }

  // When responseType is set to "blob", we redirect the downloaded data to a
  // blob directly, except for data: URLs, since those are loaded by
  // renderer side code, and don't support being downloaded to a blob.
  downloading_to_blob_ =
      GetResponseTypeCode() == kResponseTypeBlob && !url_.ProtocolIsData();
  if (downloading_to_blob_) {
    request.SetDownloadToBlob(true);
    resource_loader_options.data_buffering_policy = kDoNotBufferData;
  }

  if (async_) {
    resource_loader_options.data_buffering_policy = kDoNotBufferData;
  }

  exception_code_ = DOMExceptionCode::kNoError;
  error_ = false;

  if (async_) {
    UseCounter::Count(&execution_context,
                      WebFeature::kXMLHttpRequestAsynchronous);
    if (GetExecutionContext()->IsDocument()) {
      // Update histogram for usage of async xhr within pagedismissal.
      auto pagedismissal = GetDocument()->PageDismissalEventBeingDispatched();
      if (pagedismissal != Document::kNoDismissal) {
        UseCounter::Count(GetDocument(), WebFeature::kAsyncXhrInPageDismissal);
        DEFINE_STATIC_LOCAL(EnumerationHistogram,
                            asyncxhr_pagedismissal_histogram,
                            ("XHR.Async.PageDismissal", 5));
        asyncxhr_pagedismissal_histogram.Count(pagedismissal);
      }
    }
    if (upload_)
      request.SetReportUploadProgress(true);

    // TODO(yhirano): Turn this CHECK into DCHECK: see https://crbug.com/570946.
    CHECK(!loader_);
    DCHECK(send_flag_);
  } else {
    // Use count for XHR synchronous requests.
    UseCounter::Count(&execution_context, WebFeature::kXMLHttpRequestSynchronous);
    if (GetExecutionContext()->IsDocument()) {
      // Update histogram for usage of sync xhr within pagedismissal.
      auto pagedismissal = GetDocument()->PageDismissalEventBeingDispatched();
      if (pagedismissal != Document::kNoDismissal) {
        UseCounter::Count(GetDocument(), WebFeature::kSyncXhrInPageDismissal);
        DEFINE_STATIC_LOCAL(EnumerationHistogram, syncxhr_pagedismissal_histogram,
                            ("XHR.Sync.PageDismissal", 5));
        syncxhr_pagedismissal_histogram.Count(pagedismissal);
      }
    }
    resource_loader_options.synchronous_policy = kRequestSynchronously;
  }

  loader_ = new ThreadableLoader(execution_context, this,
                                 resource_loader_options);
  loader_->SetTimeout(timeout_);
  loader_->Start(request);

  if (!async_)
    ThrowForLoadFailureIfNeeded(exception_state, String());
}

void XMLHttpRequest::abort() {
  NETWORK_DVLOG(1) << this << " abort()";

  // internalAbort() clears the response. Save the data needed for
  // dispatching ProgressEvents.
  long long expected_length = response_.ExpectedContentLength();
  long long received_length = received_length_;

  if (!InternalAbort())
    return;

  // The script never gets any chance to call abort() on a sync XHR between
  // send() call and transition to the DONE state. It's because a sync XHR
  // doesn't dispatch any event between them. So, if |m_async| is false, we
  // can skip the "request error steps" (defined in the XHR spec) without any
  // state check.
  //
  // FIXME: It's possible open() is invoked in internalAbort() and |m_async|
  // becomes true by that. We should implement more reliable treatment for
  // nested method invocations at some point.
  if (async_) {
    if ((state_ == kOpened && send_flag_) || state_ == kHeadersReceived ||
        state_ == kLoading) {
      DCHECK(!loader_);
      HandleRequestError(DOMExceptionCode::kNoError, EventTypeNames::abort,
                         received_length, expected_length);
    }
  }
  if (state_ == kDone)
    state_ = kUnsent;
}

void XMLHttpRequest::Dispose() {
  progress_event_throttle_->Stop();
  InternalAbort();
  // TODO(yhirano): Remove this CHECK: see https://crbug.com/570946.
  CHECK(!loader_);
}

void XMLHttpRequest::ClearVariablesForLoading() {
  if (blob_loader_) {
    blob_loader_->Cancel();
    blob_loader_ = nullptr;
  }

  decoder_.reset();

  if (response_document_parser_) {
    response_document_parser_->RemoveClient(this);
    response_document_parser_->Detach();
    response_document_parser_ = nullptr;
  }
}

bool XMLHttpRequest::InternalAbort() {
  // If there is an existing pending abort event, cancel it. The caller of this
  // function is responsible for firing any events on XMLHttpRequest, if
  // needed.
  pending_abort_event_.Cancel();

  // Fast path for repeated internalAbort()s; this
  // will happen if an XHR object is notified of context
  // destruction followed by finalization.
  if (error_ && !loader_)
    return true;

  error_ = true;

  if (response_document_parser_ && !response_document_parser_->IsStopped())
    response_document_parser_->StopParsing();

  ClearVariablesForLoading();

  ClearResponse();
  ClearRequest();

  if (!loader_)
    return true;

  // Cancelling the ThreadableLoader loader_ may result in calling
  // window.onload synchronously. If such an onload handler contains open()
  // call on the same XMLHttpRequest object, reentry happens.
  //
  // If, window.onload contains open() and send(), m_loader will be set to
  // non 0 value. So, we cannot continue the outer open(). In such case,
  // just abort the outer open() by returning false.
  ThreadableLoader* loader = loader_.Release();
  loader->Cancel();

  // If abort() called internalAbort() and a nested open() ended up
  // clearing the error flag, but didn't send(), make sure the error
  // flag is still set.
  bool new_load_started = loader_;
  if (!new_load_started)
    error_ = true;

  return !new_load_started;
}

void XMLHttpRequest::ClearResponse() {
  // FIXME: when we add the support for multi-part XHR, we will have to
  // be careful with this initialization.
  received_length_ = 0;

  response_ = ResourceResponse();

  response_text_.Clear();

  parsed_response_ = false;
  response_document_ = nullptr;

  response_blob_ = nullptr;

  length_downloaded_to_blob_ = 0;
  downloading_to_blob_ = false;

  // These variables may referred by the response accessors. So, we can clear
  // this only when we clear the response holder variables above.
  binary_response_builder_ = nullptr;
  response_array_buffer_.Clear();
  response_array_buffer_failure_ = false;

  ReportMemoryUsageToV8();
}

void XMLHttpRequest::ClearRequest() {
  request_headers_.Clear();
}

void XMLHttpRequest::DispatchProgressEvent(const AtomicString& type,
                                           long long received_length,
                                           long long expected_length) {
  bool length_computable =
      expected_length > 0 && received_length <= expected_length;
  unsigned long long loaded =
      received_length >= 0 ? static_cast<unsigned long long>(received_length)
                           : 0;
  unsigned long long total =
      length_computable ? static_cast<unsigned long long>(expected_length) : 0;

  ExecutionContext* context = GetExecutionContext();
  probe::AsyncTask async_task(
      context, this, type == EventTypeNames::loadend ? nullptr : "progress",
      async_);
  progress_event_throttle_->DispatchProgressEvent(type, length_computable,
                                                  loaded, total);
}

void XMLHttpRequest::DispatchProgressEventFromSnapshot(
    const AtomicString& type) {
  DispatchProgressEvent(type, received_length_,
                        response_.ExpectedContentLength());
}

void XMLHttpRequest::HandleNetworkError() {
  NETWORK_DVLOG(1) << this << " handleNetworkError()";

  // Response is cleared next, save needed progress event data.
  long long expected_length = response_.ExpectedContentLength();
  long long received_length = received_length_;

  if (!InternalAbort())
    return;

  HandleRequestError(DOMExceptionCode::kNetworkError, EventTypeNames::error,
                     received_length, expected_length);
}

void XMLHttpRequest::HandleDidCancel() {
  NETWORK_DVLOG(1) << this << " handleDidCancel()";

  // Response is cleared next, save needed progress event data.
  long long expected_length = response_.ExpectedContentLength();
  long long received_length = received_length_;

  if (!InternalAbort())
    return;

  pending_abort_event_ = PostCancellableTask(
      *GetExecutionContext()->GetTaskRunner(TaskType::kNetworking), FROM_HERE,
      WTF::Bind(&XMLHttpRequest::HandleRequestError, WrapPersistent(this),
                DOMExceptionCode::kAbortError, EventTypeNames::abort,
                received_length, expected_length));
}

void XMLHttpRequest::HandleRequestError(DOMExceptionCode exception_code,
                                        const AtomicString& type,
                                        long long received_length,
                                        long long expected_length) {
  NETWORK_DVLOG(1) << this << " handleRequestError()";

  probe::didFinishXHR(GetExecutionContext(), this);

  send_flag_ = false;
  if (!async_) {
    DCHECK_NE(exception_code, DOMExceptionCode::kNoError);
    state_ = kDone;
    exception_code_ = exception_code;
    return;
  }

  // With m_error set, the state change steps are minimal: any pending
  // progress event is flushed + a readystatechange is dispatched.
  // No new progress events dispatched; as required, that happens at
  // the end here.
  DCHECK(error_);
  ChangeState(kDone);

  if (!upload_complete_) {
    upload_complete_ = true;
    if (upload_ && upload_events_allowed_)
      upload_->HandleRequestError(type);
  }

  // Note: The below event dispatch may be called while |hasPendingActivity() ==
  // false|, when |handleRequestError| is called after |internalAbort()|.  This
  // is safe, however, as |this| will be kept alive from a strong ref
  // |Event::m_target|.
  DispatchProgressEvent(EventTypeNames::progress, received_length,
                        expected_length);
  DispatchProgressEvent(type, received_length, expected_length);
  DispatchProgressEvent(EventTypeNames::loadend, received_length,
                        expected_length);
}

// https://xhr.spec.whatwg.org/#the-overridemimetype()-method
void XMLHttpRequest::overrideMimeType(const AtomicString& mime_type,
                                      ExceptionState& exception_state) {
  if (state_ == kLoading || state_ == kDone) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidStateError,
        "MimeType cannot be overridden when the state is LOADING or DONE.");
    return;
  }

  mime_type_override_ = "application/octet-stream";
  if (ParsedContentType(mime_type).IsValid())
    mime_type_override_ = mime_type;
}

// https://xhr.spec.whatwg.org/#the-setrequestheader()-method
void XMLHttpRequest::setRequestHeader(const AtomicString& name,
                                      const AtomicString& value,
                                      ExceptionState& exception_state) {
  // "1. If |state| is not "opened", throw an InvalidStateError exception.
  //  2. If the send() flag is set, throw an InvalidStateError exception."
  if (state_ != kOpened || send_flag_) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "The object's state must be OPENED.");
    return;
  }

  // "3. Normalize |value|."
  const String normalized_value = FetchUtils::NormalizeHeaderValue(value);

  // "4. If |name| is not a name or |value| is not a value, throw a SyntaxError
  //     exception."
  if (!IsValidHTTPToken(name)) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kSyntaxError,
        "'" + name + "' is not a valid HTTP header field name.");
    return;
  }
  if (!IsValidHTTPHeaderValue(normalized_value)) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kSyntaxError,
        "'" + normalized_value + "' is not a valid HTTP header field value.");
    return;
  }

  // "5. Terminate these steps if |name| is a forbidden header name."
  // No script (privileged or not) can set unsafe headers.
  if (CORS::IsForbiddenHeaderName(name)) {
    LogConsoleError(GetExecutionContext(),
                    "Refused to set unsafe header \"" + name + "\"");
    return;
  }

  // "6. Combine |name|/|value| in author request headers."
  SetRequestHeaderInternal(name, AtomicString(normalized_value));
}

void XMLHttpRequest::SetRequestHeaderInternal(const AtomicString& name,
                                              const AtomicString& value) {
  DCHECK_EQ(value, FetchUtils::NormalizeHeaderValue(value))
      << "Header values must be normalized";
  HTTPHeaderMap::AddResult result = request_headers_.Add(name, value);
  if (!result.is_new_entry) {
    AtomicString new_value = result.stored_value->value + ", " + value;
    result.stored_value->value = new_value;
  }
}

bool XMLHttpRequest::HasContentTypeRequestHeader() const {
  return request_headers_.Find(HTTPNames::Content_Type) !=
         request_headers_.end();
}

String XMLHttpRequest::getAllResponseHeaders() const {
  if (state_ < kHeadersReceived || error_)
    return "";

  StringBuilder string_builder;

  WebHTTPHeaderSet access_control_expose_header_set =
      WebCORS::ExtractCorsExposedHeaderNamesList(
          with_credentials_ ? network::mojom::FetchCredentialsMode::kInclude
                            : network::mojom::FetchCredentialsMode::kSameOrigin,
          WrappedResourceResponse(response_));

  HTTPHeaderMap::const_iterator end = response_.HttpHeaderFields().end();
  for (HTTPHeaderMap::const_iterator it = response_.HttpHeaderFields().begin();
       it != end; ++it) {
    // Hide any headers whose name is a forbidden response-header name.
    // This is required for all kinds of filtered responses.
    //
    // TODO: Consider removing canLoadLocalResources() call.
    // crbug.com/567527
    if (FetchUtils::IsForbiddenResponseHeaderName(it->key) &&
        !GetSecurityOrigin()->CanLoadLocalResources())
      continue;

    if (!same_origin_request_ &&
        !WebCORS::IsOnAccessControlResponseHeaderWhitelist(it->key) &&
        access_control_expose_header_set.find(it->key.Ascii().data()) ==
            access_control_expose_header_set.end())
      continue;

    string_builder.Append(it->key.LowerASCII());
    string_builder.Append(':');
    string_builder.Append(' ');
    string_builder.Append(it->value);
    string_builder.Append('\r');
    string_builder.Append('\n');
  }

  return string_builder.ToString();
}

const AtomicString& XMLHttpRequest::getResponseHeader(
    const AtomicString& name) const {
  if (state_ < kHeadersReceived || error_)
    return g_null_atom;

  // See comment in getAllResponseHeaders above.
  if (FetchUtils::IsForbiddenResponseHeaderName(name) &&
      !GetSecurityOrigin()->CanLoadLocalResources()) {
    LogConsoleError(GetExecutionContext(),
                    "Refused to get unsafe header \"" + name + "\"");
    return g_null_atom;
  }

  WebHTTPHeaderSet access_control_expose_header_set =
      WebCORS::ExtractCorsExposedHeaderNamesList(
          with_credentials_ ? network::mojom::FetchCredentialsMode::kInclude
                            : network::mojom::FetchCredentialsMode::kSameOrigin,
          WrappedResourceResponse(response_));

  if (!same_origin_request_ &&
      !WebCORS::IsOnAccessControlResponseHeaderWhitelist(name) &&
      access_control_expose_header_set.find(name.Ascii().data()) ==
          access_control_expose_header_set.end()) {
    LogConsoleError(GetExecutionContext(),
                    "Refused to get unsafe header \"" + name + "\"");
    return g_null_atom;
  }
  return response_.HttpHeaderField(name);
}

AtomicString XMLHttpRequest::FinalResponseMIMEType() const {
  AtomicString overridden_type =
      ExtractMIMETypeFromMediaType(mime_type_override_);
  if (!overridden_type.IsEmpty())
    return overridden_type;

  if (response_.IsHTTP())
    return ExtractMIMETypeFromMediaType(
        response_.HttpHeaderField(HTTPNames::Content_Type));

  return response_.MimeType();
}

AtomicString XMLHttpRequest::FinalResponseMIMETypeWithFallback() const {
  AtomicString final_type = FinalResponseMIMEType();
  if (!final_type.IsEmpty())
    return final_type;

  return AtomicString("text/xml");
}

String XMLHttpRequest::FinalResponseCharset() const {
  String override_response_charset =
      ExtractCharsetFromMediaType(mime_type_override_);
  if (!override_response_charset.IsEmpty())
    return override_response_charset;
  return response_.TextEncodingName();
}

void XMLHttpRequest::UpdateContentTypeAndCharset(
    const AtomicString& default_content_type,
    const String& charset) {
  // http://xhr.spec.whatwg.org/#the-send()-method step 4's concilliation of
  // "charset=" in any author-provided Content-Type: request header.
  String content_type = request_headers_.Get(HTTPNames::Content_Type);
  if (content_type.IsNull()) {
    SetRequestHeaderInternal(HTTPNames::Content_Type, default_content_type);
    return;
  }
  String original_content_type = content_type;
  ReplaceCharsetInMediaType(content_type, charset);
  request_headers_.Set(HTTPNames::Content_Type, AtomicString(content_type));

  if (original_content_type != content_type) {
    UseCounter::Count(GetExecutionContext(), WebFeature::kReplaceCharsetInXHR);
    if (!EqualIgnoringASCIICase(original_content_type, content_type)) {
      UseCounter::Count(GetExecutionContext(),
                        WebFeature::kReplaceCharsetInXHRIgnoringCase);
    }
  }
}

bool XMLHttpRequest::ResponseIsXML() const {
  return DOMImplementation::IsXMLMIMEType(FinalResponseMIMETypeWithFallback());
}

bool XMLHttpRequest::ResponseIsHTML() const {
  return EqualIgnoringASCIICase(FinalResponseMIMEType(), "text/html");
}

int XMLHttpRequest::status() const {
  if (state_ == kUnsent || state_ == kOpened || error_)
    return 0;

  if (response_.HttpStatusCode())
    return response_.HttpStatusCode();

  return 0;
}

String XMLHttpRequest::statusText() const {
  if (state_ == kUnsent || state_ == kOpened || error_)
    return String();

  if (!response_.HttpStatusText().IsNull())
    return response_.HttpStatusText();

  return String();
}

void XMLHttpRequest::DidFail(const ResourceError& error) {
  NETWORK_DVLOG(1) << this << " didFail()";
  ScopedEventDispatchProtect protect(&event_dispatch_recursion_level_);

  // If we are already in an error state, for instance we called abort(), bail
  // out early.
  if (error_)
    return;

  // Internally, access check violations are considered `cancellations`, but
  // at least the mixed-content and CSP specs require them to be surfaced as
  // network errors to the page. See:
  //   [1] https://www.w3.org/TR/mixed-content/#algorithms,
  //   [2] https://www.w3.org/TR/CSP3/#fetch-integration.
  if (error.IsCancellation() && !error.IsAccessCheck()) {
    HandleDidCancel();
    return;
  }

  if (error.IsTimeout()) {
    HandleDidTimeout();
    return;
  }

  HandleNetworkError();
}

void XMLHttpRequest::DidFailRedirectCheck() {
  NETWORK_DVLOG(1) << this << " didFailRedirectCheck()";
  ScopedEventDispatchProtect protect(&event_dispatch_recursion_level_);

  HandleNetworkError();
}

void XMLHttpRequest::DidFinishLoading(unsigned long identifier) {
  NETWORK_DVLOG(1) << this << " didFinishLoading(" << identifier << ")";
  ScopedEventDispatchProtect protect(&event_dispatch_recursion_level_);

  if (error_)
    return;

  if (state_ < kHeadersReceived)
    ChangeState(kHeadersReceived);

  if (downloading_to_blob_ && response_type_code_ != kResponseTypeBlob &&
      response_blob_) {
    // In this case, we have sent the request with DownloadToBlob true,
    // but the user changed the response type after that. Hence we need to
    // read the response data and provide it to this object.
    blob_loader_ =
        BlobLoader::Create(this, response_blob_->GetBlobDataHandle());
  } else {
    DidFinishLoadingInternal();
  }
}

void XMLHttpRequest::DidFinishLoadingInternal() {
  if (response_document_parser_) {
    // |DocumentParser::finish()| tells the parser that we have reached end of
    // the data.  When using |HTMLDocumentParser|, which works asynchronously,
    // we do not have the complete document just after the
    // |DocumentParser::finish()| call.  Wait for the parser to call us back in
    // |notifyParserStopped| to progress state.
    response_document_parser_->Finish();
    DCHECK(response_document_);
    return;
  }

  if (decoder_) {
    auto text = decoder_->Flush();
    if (!text.IsEmpty() && !response_text_overflow_) {
      response_text_.Concat(isolate_, text);
      response_text_overflow_ = response_text_.IsEmpty();
    }
  }

  ClearVariablesForLoading();
  EndLoading();
}

void XMLHttpRequest::DidFinishLoadingFromBlob() {
  NETWORK_DVLOG(1) << this << " didFinishLoadingFromBlob";
  ScopedEventDispatchProtect protect(&event_dispatch_recursion_level_);

  DidFinishLoadingInternal();
}

void XMLHttpRequest::DidFailLoadingFromBlob() {
  NETWORK_DVLOG(1) << this << " didFailLoadingFromBlob()";
  ScopedEventDispatchProtect protect(&event_dispatch_recursion_level_);

  if (error_)
    return;
  HandleNetworkError();
}

void XMLHttpRequest::NotifyParserStopped() {
  ScopedEventDispatchProtect protect(&event_dispatch_recursion_level_);

  // This should only be called when response document is parsed asynchronously.
  DCHECK(response_document_parser_);
  DCHECK(!response_document_parser_->IsParsing());

  // Do nothing if we are called from |internalAbort()|.
  if (error_)
    return;

  ClearVariablesForLoading();

  if (!response_document_->WellFormed())
    response_document_ = nullptr;

  parsed_response_ = true;

  EndLoading();
}

void XMLHttpRequest::EndLoading() {
  probe::didFinishXHR(GetExecutionContext(), this);

  if (loader_) {
    // Set |m_error| in order to suppress the cancel notification (see
    // XMLHttpRequest::didFail).
    base::AutoReset<bool> scope(&error_, true);
    loader_.Release()->Cancel();
  }

  send_flag_ = false;
  ChangeState(kDone);

  if (!GetExecutionContext() || !GetExecutionContext()->IsDocument())
    return;

  if (GetDocument() && GetDocument()->GetFrame() &&
      GetDocument()->GetFrame()->GetPage() && CORS::IsOkStatus(status()))
    GetDocument()->GetFrame()->GetPage()->GetChromeClient().AjaxSucceeded(
        GetDocument()->GetFrame());
}

void XMLHttpRequest::DidSendData(unsigned long long bytes_sent,
                                 unsigned long long total_bytes_to_be_sent) {
  NETWORK_DVLOG(1) << this << " didSendData(" << bytes_sent << ", "
                   << total_bytes_to_be_sent << ")";
  ScopedEventDispatchProtect protect(&event_dispatch_recursion_level_);

  if (!upload_)
    return;

  if (upload_events_allowed_)
    upload_->DispatchProgressEvent(bytes_sent, total_bytes_to_be_sent);

  if (bytes_sent == total_bytes_to_be_sent && !upload_complete_) {
    upload_complete_ = true;
    if (upload_events_allowed_)
      upload_->DispatchEventAndLoadEnd(EventTypeNames::load, true, bytes_sent,
                                       total_bytes_to_be_sent);
  }
}

void XMLHttpRequest::DidReceiveResponse(
    unsigned long identifier,
    const ResourceResponse& response,
    std::unique_ptr<WebDataConsumerHandle> handle) {
  // TODO(yhirano): Remove this CHECK: see https://crbug.com/570946.
  CHECK(&response);

  ALLOW_UNUSED_LOCAL(handle);
  DCHECK(!handle);
  NETWORK_DVLOG(1) << this << " didReceiveResponse(" << identifier << ")";
  ScopedEventDispatchProtect protect(&event_dispatch_recursion_level_);

  response_ = response;
}

void XMLHttpRequest::ParseDocumentChunk(const char* data, unsigned len) {
  if (!response_document_parser_) {
    DCHECK(!response_document_);
    InitResponseDocument();
    if (!response_document_)
      return;

    response_document_parser_ =
        response_document_->ImplicitOpen(kAllowAsynchronousParsing);
    response_document_parser_->AddClient(this);
  }
  DCHECK(response_document_parser_);

  if (response_document_parser_->NeedsDecoder())
    response_document_parser_->SetDecoder(CreateDecoder());

  response_document_parser_->AppendBytes(data, len);
}

std::unique_ptr<TextResourceDecoder> XMLHttpRequest::CreateDecoder() const {
  const TextResourceDecoderOptions decoder_options_for_utf8_plain_text(
      TextResourceDecoderOptions::kPlainTextContent, UTF8Encoding());
  if (response_type_code_ == kResponseTypeJSON)
    return TextResourceDecoder::Create(decoder_options_for_utf8_plain_text);

  String final_response_charset = FinalResponseCharset();
  if (!final_response_charset.IsEmpty()) {
    // If the final charset is given, use the charset without sniffing the
    // content.
    return TextResourceDecoder::Create(TextResourceDecoderOptions(
        TextResourceDecoderOptions::kPlainTextContent,
        WTF::TextEncoding(final_response_charset)));
  }

  TextResourceDecoderOptions decoder_options_for_xml(
      TextResourceDecoderOptions::kXMLContent);
  // Don't stop on encoding errors, unlike it is done for other kinds
  // of XML resources. This matches the behavior of previous WebKit
  // versions, Firefox and Opera.
  decoder_options_for_xml.SetUseLenientXMLDecoding();

  switch (response_type_code_) {
    case kResponseTypeDefault:
      if (ResponseIsXML())
        return TextResourceDecoder::Create(decoder_options_for_xml);
      FALLTHROUGH;
    case kResponseTypeText:
      return TextResourceDecoder::Create(decoder_options_for_utf8_plain_text);
    case kResponseTypeDocument:
      if (ResponseIsHTML()) {
        return TextResourceDecoder::Create(TextResourceDecoderOptions(
            TextResourceDecoderOptions::kHTMLContent, UTF8Encoding()));
      }
      return TextResourceDecoder::Create(decoder_options_for_xml);
    case kResponseTypeJSON:
    case kResponseTypeBlob:
    case kResponseTypeArrayBuffer:
      NOTREACHED();
      break;
  }
  NOTREACHED();
  return nullptr;
}

void XMLHttpRequest::DidReceiveData(const char* data, unsigned len) {
  ScopedEventDispatchProtect protect(&event_dispatch_recursion_level_);
  if (error_)
    return;

  DCHECK(!downloading_to_blob_ || blob_loader_);

  if (state_ < kHeadersReceived)
    ChangeState(kHeadersReceived);

  // We need to check for |m_error| again, because |changeState| may trigger
  // readystatechange, and user javascript can cause |abort()|.
  if (error_)
    return;

  if (!len)
    return;

  if (response_type_code_ == kResponseTypeDocument && ResponseIsHTML()) {
    ParseDocumentChunk(data, len);
  } else if (response_type_code_ == kResponseTypeDefault ||
             response_type_code_ == kResponseTypeText ||
             response_type_code_ == kResponseTypeJSON ||
             response_type_code_ == kResponseTypeDocument) {
    if (!decoder_)
      decoder_ = CreateDecoder();

    auto text = decoder_->Decode(data, len);
    if (!text.IsEmpty() && !response_text_overflow_) {
      response_text_.Concat(isolate_, text);
      response_text_overflow_ = response_text_.IsEmpty();
    }
  } else if (response_type_code_ == kResponseTypeArrayBuffer ||
             response_type_code_ == kResponseTypeBlob) {
    // Buffer binary data.
    if (!binary_response_builder_)
      binary_response_builder_ = SharedBuffer::Create();
    binary_response_builder_->Append(data, len);
    ReportMemoryUsageToV8();
  }

  if (blob_loader_) {
    // In this case, the data is provided by m_blobLoader. As progress
    // events are already fired, we should return here.
    return;
  }
  TrackProgress(len);
}

void XMLHttpRequest::DidDownloadData(int data_length) {
  ScopedEventDispatchProtect protect(&event_dispatch_recursion_level_);
  if (error_)
    return;

  DCHECK(downloading_to_blob_);

  if (state_ < kHeadersReceived)
    ChangeState(kHeadersReceived);

  if (!data_length)
    return;

  // readystatechange event handler may do something to put this XHR in error
  // state. We need to check m_error again here.
  if (error_)
    return;

  length_downloaded_to_blob_ += data_length;
  ReportMemoryUsageToV8();

  TrackProgress(data_length);
}

void XMLHttpRequest::DidDownloadToBlob(scoped_refptr<BlobDataHandle> blob) {
  ScopedEventDispatchProtect protect(&event_dispatch_recursion_level_);
  if (error_)
    return;

  DCHECK(downloading_to_blob_);

  if (!blob) {
    // This generally indicates not enough quota for the blob, or somehow
    // failing to write the blob to disk. Treat this as a network error.
    // TODO(mek): Maybe print a more helpful/specific error message to the
    // console, to distinguish this from true network errors?
    // TODO(mek): This would best be treated as a network error, but for sync
    // requests this could also just mean succesfully reading a zero-byte blob
    // from a misbehaving URLLoader, so for now just ignore this and don't do
    // anything, which will result in an empty blob being returned by XHR.
    // HandleNetworkError();
  } else {
    // Fix content type if overrides or fallbacks are in effect.
    String mime_type = FinalResponseMIMETypeWithFallback().LowerASCII();
    if (blob->GetType() != mime_type) {
      auto blob_size = blob->size();
      auto blob_data = BlobData::Create();
      blob_data->SetContentType(mime_type);
      blob_data->AppendBlob(std::move(blob), 0, blob_size);
      response_blob_ =
          Blob::Create(BlobDataHandle::Create(std::move(blob_data), blob_size));
    } else {
      response_blob_ = Blob::Create(std::move(blob));
    }
  }
}

void XMLHttpRequest::HandleDidTimeout() {
  NETWORK_DVLOG(1) << this << " handleDidTimeout()";

  // Response is cleared next, save needed progress event data.
  long long expected_length = response_.ExpectedContentLength();
  long long received_length = received_length_;

  if (!InternalAbort())
    return;

  HandleRequestError(DOMExceptionCode::kTimeoutError, EventTypeNames::timeout,
                     received_length, expected_length);
}

void XMLHttpRequest::Pause() {
  progress_event_throttle_->Pause();
}

void XMLHttpRequest::Unpause() {
  progress_event_throttle_->Unpause();
}

void XMLHttpRequest::ContextDestroyed(ExecutionContext*) {
  Dispose();

  // In case we are in the middle of send() function, unset the send flag to
  // stop the operation.
  send_flag_ = false;
}

bool XMLHttpRequest::HasPendingActivity() const {
  // Neither this object nor the JavaScript wrapper should be deleted while
  // a request is in progress because we need to keep the listeners alive,
  // and they are referenced by the JavaScript wrapper.
  // |m_loader| is non-null while request is active and ThreadableLoaderClient
  // callbacks may be called, and |m_responseDocumentParser| is non-null while
  // DocumentParserClient callbacks may be called.
  if (loader_ || response_document_parser_)
    return true;
  return event_dispatch_recursion_level_ > 0;
}

const AtomicString& XMLHttpRequest::InterfaceName() const {
  return EventTargetNames::XMLHttpRequest;
}

ExecutionContext* XMLHttpRequest::GetExecutionContext() const {
  return PausableObject::GetExecutionContext();
}

void XMLHttpRequest::ReportMemoryUsageToV8() {
  // binary_response_builder_
  size_t size = binary_response_builder_ ? binary_response_builder_->size() : 0;
  int64_t diff =
      static_cast<int64_t>(size) -
      static_cast<int64_t>(binary_response_builder_last_reported_size_);
  binary_response_builder_last_reported_size_ = size;

  // Blob (downloading_to_blob_, length_downloaded_to_blob_)
  diff += static_cast<int64_t>(length_downloaded_to_blob_) -
          static_cast<int64_t>(length_downloaded_to_blob_last_reported_);
  length_downloaded_to_blob_last_reported_ = length_downloaded_to_blob_;

  if (diff)
    isolate_->AdjustAmountOfExternalAllocatedMemory(diff);
}

void XMLHttpRequest::Trace(blink::Visitor* visitor) {
  visitor->Trace(response_blob_);
  visitor->Trace(loader_);
  visitor->Trace(response_document_);
  visitor->Trace(response_document_parser_);
  visitor->Trace(response_array_buffer_);
  visitor->Trace(progress_event_throttle_);
  visitor->Trace(upload_);
  visitor->Trace(blob_loader_);
  visitor->Trace(response_text_);
  XMLHttpRequestEventTarget::Trace(visitor);
  DocumentParserClient::Trace(visitor);
  PausableObject::Trace(visitor);
}

std::ostream& operator<<(std::ostream& ostream, const XMLHttpRequest* xhr) {
  return ostream << "XMLHttpRequest " << static_cast<const void*>(xhr);
}

}  // namespace blink
