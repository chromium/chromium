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
#include <optional>
#include <utility>

#include "base/auto_reset.h"
#include "base/containers/contains.h"
#include "base/containers/span.h"
#include "base/feature_list.h"
#include "base/numerics/safe_conversions.h"
#include "base/time/time.h"
#include "net/base/mime_util.h"
#include "services/network/public/cpp/header_util.h"
#include "services/network/public/mojom/trust_tokens.mojom-shared.h"
#include "third_party/blink/public/common/blob/blob_utils.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/mojom/fetch/fetch_api_request.mojom-blink.h"
#include "third_party/blink/public/mojom/permissions_policy/permissions_policy.mojom-blink-forward.h"
#include "third_party/blink/public/mojom/permissions_policy/permissions_policy.mojom-blink.h"
#include "third_party/blink/public/platform/web_url_request.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_private_token.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_union_arraybuffer_arraybufferview_blob_document_formdata_urlsearchparams_usvstring.h"
#include "third_party/blink/renderer/core/dom/document_init.h"
#include "third_party/blink/renderer/core/dom/document_parser.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/dom/xml_document.h"
#include "third_party/blink/renderer/core/editing/serializers/serialization.h"
#include "third_party/blink/renderer/core/event_target_names.h"
#include "third_party/blink/renderer/core/events/progress_event.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/fetch/attribution_reporting_to_mojom.h"
#include "third_party/blink/renderer/core/fetch/trust_token_to_mojom.h"
#include "third_party/blink/renderer/core/fileapi/blob.h"
#include "third_party/blink/renderer/core/fileapi/file.h"
#include "third_party/blink/renderer/core/fileapi/file_reader_client.h"
#include "third_party/blink/renderer/core/fileapi/file_reader_loader.h"
#include "third_party/blink/renderer/core/fileapi/public_url_manager.h"
#include "third_party/blink/renderer/core/frame/csp/content_security_policy.h"
#include "third_party/blink/renderer/core/frame/deprecation/deprecation.h"
#include "third_party/blink/renderer/core/frame/frame.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/page_dismissal_scope.h"
#include "third_party/blink/renderer/core/frame/settings.h"
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
#include "third_party/blink/renderer/platform/file_metadata.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
#include "third_party/blink/renderer/platform/loader/cors/cors.h"
#include "third_party/blink/renderer/platform/loader/fetch/fetch_initiator_type_names.h"
#include "third_party/blink/renderer/platform/loader/fetch/fetch_utils.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_error.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_loader_options.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_request.h"
#include "third_party/blink/renderer/platform/loader/fetch/text_resource_decoder_options.h"
#include "third_party/blink/renderer/platform/network/http_names.h"
#include "third_party/blink/renderer/platform/network/http_parsers.h"
#include "third_party/blink/renderer/platform/network/mime/mime_type_registry.h"
#include "third_party/blink/renderer/platform/network/parsed_content_type.h"
#include "third_party/blink/renderer/platform/scheduler/public/task_attribution_tracker.h"
#include "third_party/blink/renderer/platform/weborigin/scheme_registry.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"
#include "third_party/blink/renderer/platform/weborigin/security_policy.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/shared_buffer.h"
#include "third_party/blink/renderer/platform/wtf/std_lib_extras.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace blink {

namespace {

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

  unsigned pos = charset_pos;
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
  auto* console_message = MakeGarbageCollected<ConsoleMessage>(
      mojom::ConsoleMessageSource::kJavaScript,
      mojom::ConsoleMessageLevel::kError, message);
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
    : public GarbageCollected<XMLHttpRequest::BlobLoader>,
      public FileReaderClient {
 public:
  BlobLoader(XMLHttpRequest* xhr, scoped_refptr<BlobDataHandle> handle)
      : xhr_(xhr),
        loader_(MakeGarbageCollected<FileReaderLoader>(
            this,
            xhr->GetExecutionContext()->GetTaskRunner(
                TaskType::kFileReading))) {
    loader_->Start(std::move(handle));
  }

  // FileReaderClient functions.
  FileErrorCode DidStartLoading(uint64_t) override {
    return FileErrorCode::kOK;
  }
  FileErrorCode DidReceiveData(base::span<const uint8_t> data) override {
    DCHECK_LE(data.size(), static_cast<size_t>(INT_MAX));
    xhr_->DidReceiveData(base::as_chars(data));
    return FileErrorCode::kOK;
  }
  void DidFinishLoading() override { xhr_->DidFinishLoadingFromBlob(); }
  void DidFail(FileErrorCode error) override { xhr_->DidFailLoadingFromBlob(); }

  void Cancel() { loader_->Cancel(); }

  void Trace(Visitor* visitor) const override {
    FileReaderClient::Trace(visitor);
    visitor->Trace(xhr_);
    visitor->Trace(loader_);
  }

 private:
  Member<XMLHttpRequest> xhr_;
  Member<FileReaderLoader> loader_;
};

XMLHttpRequest* XMLHttpRequest::Create(ScriptState* script_state) {
  return MakeGarbageCollected<XMLHttpRequest>(
      ExecutionContext::From(script_state), &script_state->World());
}

XMLHttpRequest* XMLHttpRequest::Create(ExecutionContext* context) {
  return MakeGarbageCollected<XMLHttpRequest>(context, nullptr);
}

XMLHttpRequest::XMLHttpRequest(ExecutionContext* context,
                               const DOMWrapperWorld* world)
    : ActiveScriptWrappable<XMLHttpRequest>({}),
      ExecutionContextLifecycleObserver(context),
      progress_event_throttle_(
          MakeGarbageCollected<XMLHttpRequestProgressEventThrottle>(this)),
      world_(world),
      isolated_world_security_origin_(world_ && world_->IsIsolatedWorld()
                                          ? world_->IsolatedWorldSecurityOrigin(
                                                context->GetAgentClusterID())
                                          : nullptr) {}

XMLHttpRequest::~XMLHttpRequest() {
  binary_response_builder_ = nullptr;
  length_downloaded_to_blob_ = 0;
  response_text_.Clear();
  ReportMemoryUsageToV8();
}

XMLHttpRequest::State XMLHttpRequest::readyState() const {
  return state_;
}

String XMLHttpRequest::responseText(ExceptionState& exception_state) {
  if (response_type_code_ != kResponseTypeDefault &&
      response_type_code_ != V8XMLHttpRequestResponseType::Enum::kText) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "The value is only accessible if the "
                                      "object's 'responseType' is '' or 'text' "
                                      "(was '" +
                                          responseType().AsString() + "').");
    return String();
  }
  if (error_ || (state_ != kLoading && state_ != kDone))
    return String();
  return response_text_.ToString();
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
                          .WithExecutionContext(GetExecutionContext())
                          .WithAgent(*GetExecutionContext()->GetAgent())
                          .WithURL(response_.ResponseUrl());
  if (is_html) {
    response_document_ = MakeGarbageCollected<HTMLDocument>(init);
    response_document_->setAllowDeclarativeShadowRoots(false);
  } else
    response_document_ = MakeGarbageCollected<XMLDocument>(init);

  // FIXME: Set Last-Modified.
  response_document_->SetMimeType(GetResponseMIMEType());
}

Document* XMLHttpRequest::responseXML(ExceptionState& exception_state) {
  if (response_type_code_ != kResponseTypeDefault &&
      response_type_code_ != V8XMLHttpRequestResponseType::Enum::kDocument) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "The value is only accessible if the "
                                      "object's 'responseType' is '' or "
                                      "'document' (was '" +
                                          responseType().AsString() + "').");
    return nullptr;
  }

  if (error_ || state_ != kDone)
    return nullptr;

  if (!parsed_response_) {
    InitResponseDocument();
    if (!response_document_)
      return nullptr;

    response_document_->SetContent(response_text_.ToString());
    if (!response_document_->WellFormed()) {
      response_document_ = nullptr;
    } else {
      response_document_->OverrideLastModified(
          response_.HttpHeaderField(http_names::kLastModified));
    }

    parsed_response_ = true;
  }

  return response_document_.Get();
}

v8::Local<v8::Value> XMLHttpRequest::ResponseJSON(
    v8::Isolate* isolate,
    ExceptionState& exception_state) {
  DCHECK_EQ(response_type_code_, V8XMLHttpRequestResponseType::Enum::kJson);
  DCHECK(!error_);
  DCHECK_EQ(state_, kDone);
  TryRethrowScope rethrow_scope(isolate, exception_state);
  // Catch syntax error. Swallows an exception (when thrown) as the
  // spec says. https://xhr.spec.whatwg.org/#response-body
  v8::Local<v8::Value> json =
      FromJSONString(isolate, isolate->GetCurrentContext(),
                     response_text_.ToString(), rethrow_scope);
  if (rethrow_scope.HasCaught()) {
    rethrow_scope.SwallowException();
    return v8::Null(isolate);
  }
  return json;
}

Blob* XMLHttpRequest::ResponseBlob() {
  DCHECK_EQ(response_type_code_, V8XMLHttpRequestResponseType::Enum::kBlob);
  DCHECK(!error_);
  DCHECK_EQ(state_, kDone);

  if (!response_blob_) {
    auto blob_data = std::make_unique<BlobData>();
    blob_data->SetContentType(GetResponseMIMEType().LowerASCII());
    size_t size = 0;
    if (binary_response_builder_ && binary_response_builder_->size()) {
      for (const auto& span : *binary_response_builder_)
        blob_data->AppendBytes(base::as_bytes(span));
      size = binary_response_builder_->size();
      binary_response_builder_ = nullptr;
      ReportMemoryUsageToV8();
    }
    response_blob_ = MakeGarbageCollected<Blob>(
        BlobDataHandle::Create(std::move(blob_data), size));
  }

  return response_blob_.Get();
}

DOMArrayBuffer* XMLHttpRequest::ResponseArrayBuffer() {
  DCHECK_EQ(response_type_code_,
            V8XMLHttpRequestResponseType::Enum::kArraybuffer);
  DCHECK(!error_);
  DCHECK_EQ(state_, kDone);

  if (!response_array_buffer_ && !response_array_buffer_failure_) {
    if (binary_response_builder_ && binary_response_builder_->size()) {
      DOMArrayBuffer* buffer = DOMArrayBuffer::CreateUninitializedOrNull(
          binary_response_builder_->size(), 1);
      if (buffer) {
        bool result = binary_response_builder_->GetBytes(buffer->Data(),
                                                         buffer->ByteLength());
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
      response_array_buffer_ = DOMArrayBuffer::Create(base::span<uint8_t>());
    }
  }

  return response_array_buffer_.Get();
}

// https://xhr.spec.whatwg.org/#dom-xmlhttprequest-response
ScriptValue XMLHttpRequest::response(ScriptState* script_state,
                                     ExceptionState& exception_state) {
  v8::Isolate* isolate = script_state->GetIsolate();

  // The spec handles default or `text` responses as a special case, because
  // these cases are allowed to access the response while still loading.
  if (response_type_code_ == kResponseTypeDefault ||
      response_type_code_ == V8XMLHttpRequestResponseType::Enum::kText) {
    const auto& text = responseText(exception_state);
    if (exception_state.HadException()) {
      return ScriptValue();
    }
    return ScriptValue(isolate,
                       ToV8Traits<IDLString>::ToV8(script_state, text));
  }

  if (error_ || state_ != kDone) {
    return ScriptValue(isolate, v8::Null(isolate));
  }

  switch (response_type_code_) {
    case V8XMLHttpRequestResponseType::Enum::kJson:
      return ScriptValue(isolate, ResponseJSON(isolate, exception_state));
    case V8XMLHttpRequestResponseType::Enum::kDocument: {
      Document* document = responseXML(exception_state);
      if (exception_state.HadException()) {
        return ScriptValue();
      }
      return ScriptValue(isolate, ToV8Traits<IDLNullable<Document>>::ToV8(
                                      script_state, document));
    }
    case V8XMLHttpRequestResponseType::Enum::kBlob:
      return ScriptValue(isolate,
                         ToV8Traits<Blob>::ToV8(script_state, ResponseBlob()));
    case V8XMLHttpRequestResponseType::Enum::kArraybuffer:
      return ScriptValue(isolate, ToV8Traits<IDLNullable<DOMArrayBuffer>>::ToV8(
                                      script_state, ResponseArrayBuffer()));
    default:
      NOTREACHED_IN_MIGRATION();
      return ScriptValue();
  }
}

void XMLHttpRequest::setTimeout(unsigned timeout,
                                ExceptionState& exception_state) {
  // FIXME: Need to trigger or update the timeout Timer here, if needed.
  // http://webkit.org/b/98156
  // XHR2 spec, 4.7.3. "This implies that the timeout attribute can be set while
  // fetching is in progress. If that occurs it will still be measured relative
  // to the start of fetching."
  if (GetExecutionContext() && GetExecutionContext()->IsWindow() && !async_) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidAccessError,
                                      "Timeouts cannot be set for synchronous "
                                      "requests made from a document.");
    return;
  }

  timeout_ = base::Milliseconds(timeout);

  // From http://www.w3.org/TR/XMLHttpRequest/#the-timeout-attribute:
  // Note: This implies that the timeout attribute can be set while fetching is
  // in progress. If that occurs it will still be measured relative to the start
  // of fetching.
  //
  // The timeout may be overridden after send.
  if (loader_)
    loader_->SetTimeout(timeout_);
}

void XMLHttpRequest::setResponseType(
    const V8XMLHttpRequestResponseType& response_type,
    ExceptionState& exception_state) {
  const bool is_window =
      GetExecutionContext() && GetExecutionContext()->IsWindow();
  // 1. If the current global object is not a Window object and the given value
  // is "document", then return.
  if (!is_window && response_type == "document") {
    return;
  }

  // 2. If this’s state is loading or done, then throw an "InvalidStateError"
  // DOMException.
  if (state_ >= kLoading) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "The response type cannot be set if the "
                                      "object's state is LOADING or DONE.");
    return;
  }

  // Newer functionality is not available to synchronous requests in window
  // contexts, as a spec-mandated attempt to discourage synchronous XHR use.
  // responseType is one such piece of functionality.
  if (is_window && !async_) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidAccessError,
                                      "The response type cannot be changed for "
                                      "synchronous requests made from a "
                                      "document.");
    return;
  }

  response_type_code_ = response_type.AsEnum();
}

V8XMLHttpRequestResponseType XMLHttpRequest::responseType() {
  return V8XMLHttpRequestResponseType(response_type_code_);
}

String XMLHttpRequest::responseURL() {
  KURL response_url(response_.ResponseUrl());
  if (!response_url.IsNull())
    response_url.RemoveFragmentIdentifier();
  return response_url.GetString();
}

XMLHttpRequestUpload* XMLHttpRequest::upload() {
  if (!upload_)
    upload_ = MakeGarbageCollected<XMLHttpRequestUpload>(this);
  return upload_.Get();
}

void XMLHttpRequest::TrackProgress(uint64_t length) {
  received_length_ += length;

  ChangeState(kLoading);
  if (async_) {
    // readyStateChange event is fired as well.
    DispatchProgressEventFromSnapshot(event_type_names::kProgress);
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

  if (async_ || (state_ <= kOpened || state_ == kDone)) {
    DEVTOOLS_TIMELINE_TRACE_EVENT("XHRReadyStateChange",
                                  inspector_xhr_ready_state_change_event::Data,
                                  GetExecutionContext(), this);
    XMLHttpRequestProgressEventThrottle::DeferredEventAction action =
        XMLHttpRequestProgressEventThrottle::kIgnore;
    if (state_ == kDone) {
      if (error_)
        action = XMLHttpRequestProgressEventThrottle::kClear;
      else
        action = XMLHttpRequestProgressEventThrottle::kFlush;
    }
    std::optional<scheduler::TaskAttributionTracker::TaskScope>
        task_attribution_scope = MaybeCreateTaskAttributionScope();
    progress_event_throttle_->DispatchReadyStateChangeEvent(
        Event::Create(event_type_names::kReadystatechange), action);
  }

  if (state_ == kDone && !error_) {
    DEVTOOLS_TIMELINE_TRACE_EVENT("XHRLoad", inspector_xhr_load_event::Data,
                                  GetExecutionContext(), this);
    DispatchProgressEventFromSnapshot(event_type_names::kLoad);
    DispatchProgressEventFromSnapshot(event_type_names::kLoadend);
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
  DVLOG(1) << this << " open(" << method << ", " << url.ElidedString() << ", "
           << async << ")";

  DCHECK(ValidateOpenArguments(method, url, exception_state));

  InternalAbort();

  State previous_state = state_;
  state_ = kUnsent;
  error_ = false;
  upload_complete_ = false;
  parent_task_ = nullptr;

  auto* window = DynamicTo<LocalDOMWindow>(GetExecutionContext());
  if (!async && window) {
    if (window->GetFrame() &&
        !window->GetFrame()->GetSettings()->GetSyncXHRInDocumentsEnabled()) {
      exception_state.ThrowDOMException(
          DOMExceptionCode::kInvalidAccessError,
          "Synchronous requests are disabled for this page.");
      return;
    }

    // Newer functionality is not available to synchronous requests in window
    // contexts, as a spec-mandated attempt to discourage synchronous XHR use.
    // responseType is one such piece of functionality.
    if (response_type_code_ != V8XMLHttpRequestResponseType::Enum::k) {
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
    if (!window->document()->ProcessingBeforeUnload()) {
      Deprecation::CountDeprecation(
          GetExecutionContext(),
          WebFeature::kXMLHttpRequestSynchronousInNonWorkerOutsideBeforeUnload);
    }
  }

  method_ = FetchUtils::NormalizeMethod(method);

  url_ = url;

  if (url_.ProtocolIs("blob")) {
    GetExecutionContext()->GetPublicURLManager().Resolve(
        url_, blob_url_loader_factory_.InitWithNewPipeAndPassReceiver());
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
    if (GetExecutionContext()->IsWindow()) {
      bool sync_xhr_disabled_by_permissions_policy =
          !GetExecutionContext()->IsFeatureEnabled(
              mojom::blink::PermissionsPolicyFeature::kSyncXHR,
              ReportOptions::kReportOnFailure,
              "Synchronous requests are disabled by permissions policy.");

      bool sync_xhr_disabled_by_document_policy =
          !GetExecutionContext()->IsFeatureEnabled(
              mojom::blink::DocumentPolicyFeature::kSyncXHR,
              ReportOptions::kReportOnFailure,
              "Synchronous requests are disabled by document policy.");

      // SyncXHR can be controlled by either permissions policy or document
      // policy during the migration period. See crbug.com/1146505.
      if (sync_xhr_disabled_by_permissions_policy ||
          sync_xhr_disabled_by_document_policy) {
        HandleNetworkError();
        ThrowForLoadFailureIfNeeded(exception_state, String());
        return false;
      }
    }
    v8::Isolate* isolate = GetExecutionContext()->GetIsolate();
    v8::MicrotaskQueue* microtask_queue =
        ToMicrotaskQueue(GetExecutionContext());
    if (isolate &&
        ((microtask_queue && microtask_queue->IsRunningMicrotasks()) ||
         (!microtask_queue &&
          v8::MicrotasksScope::IsRunningMicrotasks(isolate)))) {
      UseCounter::Count(GetExecutionContext(),
                        WebFeature::kDuring_Microtask_SyncXHR);
    }
  }

  error_ = false;
  return true;
}

void XMLHttpRequest::send(const V8UnionDocumentOrXMLHttpRequestBodyInit* body,
                          ExceptionState& exception_state) {
  probe::WillSendXMLHttpOrFetchNetworkRequest(GetExecutionContext(), Url());

  if (!body)
    return send(String(), exception_state);

  switch (body->GetContentType()) {
    case V8UnionDocumentOrXMLHttpRequestBodyInit::ContentType::kArrayBuffer:
      return send(body->GetAsArrayBuffer(), exception_state);
    case V8UnionDocumentOrXMLHttpRequestBodyInit::ContentType::kArrayBufferView:
      return send(body->GetAsArrayBufferView().Get(), exception_state);
    case V8UnionDocumentOrXMLHttpRequestBodyInit::ContentType::kBlob:
      return send(body->GetAsBlob(), exception_state);
    case V8UnionDocumentOrXMLHttpRequestBodyInit::ContentType::kDocument:
      return send(body->GetAsDocument(), exception_state);
    case V8UnionDocumentOrXMLHttpRequestBodyInit::ContentType::kFormData:
      return send(body->GetAsFormData(), exception_state);
    case V8UnionDocumentOrXMLHttpRequestBodyInit::ContentType::kURLSearchParams:
      return send(body->GetAsURLSearchParams(), exception_state);
    case V8UnionDocumentOrXMLHttpRequestBodyInit::ContentType::kUSVString:
      return send(body->GetAsUSVString(), exception_state);
  }

  NOTREACHED_IN_MIGRATION();
}

bool XMLHttpRequest::AreMethodAndURLValidForSend() {
  return method_ != http_names::kGET && method_ != http_names::kHEAD &&
         SchemeRegistry::ShouldTreatURLSchemeAsSupportingFetchAPI(
             url_.Protocol());
}

void XMLHttpRequest::send(Document* document, ExceptionState& exception_state) {
  DVLOG(1) << this << " send() Document " << static_cast<void*>(document);

  DCHECK(document);

  if (!InitSend(exception_state))
    return;

  scoped_refptr<EncodedFormData> http_body;

  if (AreMethodAndURLValidForSend()) {
    if (IsA<HTMLDocument>(document)) {
      UpdateContentTypeAndCharset(AtomicString("text/html;charset=UTF-8"),
                                  "UTF-8");
    } else if (IsA<XMLDocument>(document)) {
      UpdateContentTypeAndCharset(AtomicString("application/xml;charset=UTF-8"),
                                  "UTF-8");
    }

    String body = CreateMarkup(document);

    http_body = EncodedFormData::Create(
        UTF8Encoding().Encode(body, WTF::kNoUnencodables));
  }

  CreateRequest(std::move(http_body), exception_state);
}

void XMLHttpRequest::send(const String& body, ExceptionState& exception_state) {
  DVLOG(1) << this << " send() String " << body;

  if (!InitSend(exception_state))
    return;

  scoped_refptr<EncodedFormData> http_body;

  if (!body.IsNull() && AreMethodAndURLValidForSend()) {
    http_body = EncodedFormData::Create(
        UTF8Encoding().Encode(body, WTF::kNoUnencodables));
    UpdateContentTypeAndCharset(AtomicString("text/plain;charset=UTF-8"),
                                "UTF-8");
  }

  CreateRequest(std::move(http_body), exception_state);
}

void XMLHttpRequest::send(Blob* body, ExceptionState& exception_state) {
  DVLOG(1) << this << " send() Blob " << body->Uuid();

  if (!InitSend(exception_state))
    return;

  scoped_refptr<EncodedFormData> http_body;

  if (AreMethodAndURLValidForSend()) {
    if (!HasContentTypeRequestHeader()) {
      const String& blob_type = FetchUtils::NormalizeHeaderValue(body->type());
      if (!blob_type.empty() && ParsedContentType(blob_type).IsValid()) {
        SetRequestHeaderInternal(http_names::kContentType,
                                 AtomicString(blob_type));
      }
    }

    // FIXME: add support for uploading bundles.
    http_body = EncodedFormData::Create();
    if (body->HasBackingFile()) {
      auto* file = To<File>(body);
      if (!file->GetPath().empty())
        http_body->AppendFile(file->GetPath(), file->LastModifiedTime());
      else
        NOTREACHED_IN_MIGRATION();
    } else {
      http_body->AppendBlob(body->Uuid(), body->GetBlobDataHandle());
    }
  }

  CreateRequest(std::move(http_body), exception_state);
}

void XMLHttpRequest::send(FormData* body, ExceptionState& exception_state) {
  DVLOG(1) << this << " send() FormData " << body;

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
      SetRequestHeaderInternal(http_names::kContentType, content_type);
    }
  }

  CreateRequest(std::move(http_body), exception_state);
}

void XMLHttpRequest::send(URLSearchParams* body,
                          ExceptionState& exception_state) {
  DVLOG(1) << this << " send() URLSearchParams " << body;

  if (!InitSend(exception_state))
    return;

  scoped_refptr<EncodedFormData> http_body;

  if (AreMethodAndURLValidForSend()) {
    http_body = body->ToEncodedFormData();
    UpdateContentTypeAndCharset(
        AtomicString("application/x-www-form-urlencoded;charset=UTF-8"),
        "UTF-8");
  }

  CreateRequest(std::move(http_body), exception_state);
}

void XMLHttpRequest::send(DOMArrayBuffer* body,
                          ExceptionState& exception_state) {
  DVLOG(1) << this << " send() ArrayBuffer " << body;

  SendBytesData(body->Data(), body->ByteLength(), exception_state);
}

void XMLHttpRequest::send(DOMArrayBufferView* body,
                          ExceptionState& exception_state) {
  DVLOG(1) << this << " send() ArrayBufferView " << body;

  SendBytesData(body->BaseAddress(), body->byteLength(), exception_state);
}

void XMLHttpRequest::SendBytesData(const void* data,
                                   size_t length,
                                   ExceptionState& exception_state) {
  if (!InitSend(exception_state))
    return;

  scoped_refptr<EncodedFormData> http_body;

  if (AreMethodAndURLValidForSend()) {
    http_body =
        EncodedFormData::Create(data, base::checked_cast<wtf_size_t>(length));
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

  StringBuilder message;
  message.Append("Failed to load '");
  message.Append(url_.ElidedString());
  message.Append('\'');
  if (reason.IsNull()) {
    message.Append('.');
  } else {
    message.Append(": ");
    message.Append(reason);
  }

  exception_state.ThrowDOMException(exception_code_, message.ToString());
}

void XMLHttpRequest::CreateRequest(scoped_refptr<EncodedFormData> http_body,
                                   ExceptionState& exception_state) {
  // Only GET request is supported for blob URL.
  if (url_.ProtocolIs("blob") && method_ != http_names::kGET) {
    HandleNetworkError();

    if (!async_) {
      ThrowForLoadFailureIfNeeded(
          exception_state,
          "'GET' is the only method allowed for 'blob:' URLs.");
    }
    return;
  }

  if (url_.ProtocolIs("ftp")) {
    LogConsoleError(GetExecutionContext(), "FTP is not supported.");
    HandleNetworkError();
    if (!async_) {
      ThrowForLoadFailureIfNeeded(
          exception_state, "Making a request to a FTP URL is not supported.");
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
    CHECK(!execution_context.IsContextDestroyed());
    if (world_ && world_->IsMainWorld()) {
      if (auto* tracker = scheduler::TaskAttributionTracker::From(
              execution_context.GetIsolate())) {
        parent_task_ = tracker->RunningTask();
      }
    }
    async_task_context_.Schedule(&execution_context, "XMLHttpRequest.send");
    DispatchProgressEvent(event_type_names::kLoadstart, 0, 0);
    // Event handler could have invalidated this send operation,
    // (re)setting the send flag and/or initiating another send
    // operation; leave quietly if so.
    if (!send_flag_ || loader_)
      return;
    if (http_body && upload_) {
      upload_events = upload_->HasEventListeners();
      upload_->DispatchEvent(*ProgressEvent::Create(
          event_type_names::kLoadstart, true, 0, http_body->SizeInBytes()));
      // See above.
      if (!send_flag_ || loader_)
        return;
    }
  }

  // We also remember whether upload events should be allowed for this request
  // in case the upload listeners are added after the request is started.
  upload_events_allowed_ =
      GetExecutionContext()->GetSecurityOrigin()->CanRequest(url_) ||
      (isolated_world_security_origin_ &&
       isolated_world_security_origin_->CanRequest(url_)) ||
      upload_events || !cors::IsCorsSafelistedMethod(method_) ||
      !cors::ContainsOnlyCorsSafelistedHeaders(request_headers_);

  ResourceRequest request(url_);
  request.SetRequestorOrigin(GetExecutionContext()->GetSecurityOrigin());
  request.SetIsolatedWorldOrigin(isolated_world_security_origin_);
  request.SetHttpMethod(method_);
  request.SetRequestContext(mojom::blink::RequestContextType::XML_HTTP_REQUEST);
  request.SetFetchLikeAPI(true);
  request.SetMode(upload_events
                      ? network::mojom::RequestMode::kCorsWithForcedPreflight
                      : network::mojom::RequestMode::kCors);
  request.SetTargetAddressSpace(network::mojom::IPAddressSpace::kUnknown);
  request.SetCredentialsMode(
      with_credentials_ ? network::mojom::CredentialsMode::kInclude
                        : network::mojom::CredentialsMode::kSameOrigin);
  request.SetSkipServiceWorker(world_ && world_->IsIsolatedWorld());
  if (trust_token_params_)
    request.SetTrustTokenParams(*trust_token_params_);

  request.SetAttributionReportingEligibility(
      attribution_reporting_eligibility_);

  probe::WillLoadXHR(&execution_context, method_, url_, async_,
                     request_headers_, with_credentials_);

  if (http_body) {
    DCHECK_NE(method_, http_names::kGET);
    DCHECK_NE(method_, http_names::kHEAD);
    request.SetHttpBody(std::move(http_body));
  }

  if (request_headers_.size() > 0)
    request.AddHTTPHeaderFields(request_headers_);

  ResourceLoaderOptions resource_loader_options(world_);
  resource_loader_options.initiator_info.name =
      fetch_initiator_type_names::kXmlhttprequest;
  if (blob_url_loader_factory_) {
    resource_loader_options.url_loader_factory =
        base::MakeRefCounted<base::RefCountedData<
            mojo::PendingRemote<network::mojom::blink::URLLoaderFactory>>>(
            std::move(blob_url_loader_factory_));
  }

  // When responseType is set to "blob", we redirect the downloaded data to a
  // blob directly, except for data: URLs, since those are loaded by
  // renderer side code, and don't support being downloaded to a blob.
  downloading_to_blob_ =
      GetResponseTypeCode() == V8XMLHttpRequestResponseType::Enum::kBlob &&
      !url_.ProtocolIsData();
  if (downloading_to_blob_) {
    request.SetDownloadToBlob(true);
    resource_loader_options.data_buffering_policy = kDoNotBufferData;
  }

  if (async_) {
    resource_loader_options.data_buffering_policy = kDoNotBufferData;
  }

  if (async_) {
    UseCounter::Count(&execution_context,
                      WebFeature::kXMLHttpRequestAsynchronous);
    if (upload_)
      request.SetReportUploadProgress(true);

    // TODO(yhirano): Turn this CHECK into DCHECK: see https://crbug.com/570946.
    CHECK(!loader_);
    DCHECK(send_flag_);
  } else {
    // Use count for XHR synchronous requests.
    UseCounter::Count(&execution_context, WebFeature::kXMLHttpRequestSynchronous);
    if (auto* window = DynamicTo<LocalDOMWindow>(GetExecutionContext())) {
      if (Frame* frame = window->GetFrame()) {
        if (frame->IsCrossOriginToOutermostMainFrame()) {
          UseCounter::Count(
              &execution_context,
              WebFeature::kXMLHttpRequestSynchronousInCrossOriginSubframe);
        } else if (frame->IsMainFrame()) {
          UseCounter::Count(&execution_context,
                            WebFeature::kXMLHttpRequestSynchronousInMainFrame);
        } else {
          UseCounter::Count(
              &execution_context,
              WebFeature::kXMLHttpRequestSynchronousInSameOriginSubframe);
        }
      }
      if (PageDismissalScope::IsActive()) {
        HandleNetworkError();
        ThrowForLoadFailureIfNeeded(exception_state,
                                    "Synchronous XHR in page dismissal. See "
                                    "https://www.chromestatus.com/feature/"
                                    "4664843055398912 for more details.");
        return;
      }
    } else {
      DCHECK(execution_context.IsWorkerGlobalScope());
      UseCounter::Count(&execution_context,
                        WebFeature::kXMLHttpRequestSynchronousInWorker);
    }
    resource_loader_options.synchronous_policy = kRequestSynchronously;
  }

  exception_code_ = DOMExceptionCode::kNoError;
  error_ = false;

  loader_ = MakeGarbageCollected<ThreadableLoader>(execution_context, this,
                                                   resource_loader_options);
  loader_->SetTimeout(timeout_);
  base::TimeTicks start_time = base::TimeTicks::Now();
  loader_->Start(std::move(request));

  if (!async_) {
    base::TimeDelta blocking_time = base::TimeTicks::Now() - start_time;

    probe::DidFinishSyncXHR(&execution_context, blocking_time);

    ThrowForLoadFailureIfNeeded(exception_state, String());
  }
}

void XMLHttpRequest::abort() {
  DVLOG(1) << this << " abort()";

  InternalAbort();

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
      HandleRequestError(DOMExceptionCode::kNoError, event_type_names::kAbort);
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

void XMLHttpRequest::InternalAbort() {
  // If there is an existing pending abort event, cancel it. The caller of this
  // function is responsible for firing any events on XMLHttpRequest, if
  // needed.
  pending_abort_event_.Cancel();

  // Fast path for repeated internalAbort()s; this
  // will happen if an XHR object is notified of context
  // destruction followed by finalization.
  if (error_ && !loader_)
    return;

  error_ = true;

  if (response_document_parser_ && !response_document_parser_->IsStopped())
    response_document_parser_->StopParsing();

  ClearVariablesForLoading();

  ClearResponse();
  ClearRequest();

  if (!loader_)
    return;

  ThreadableLoader* loader = loader_.Release();
  loader->Cancel();

  DCHECK(!loader_);
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
                                           int64_t received_length,
                                           int64_t expected_length) {
  bool length_computable =
      expected_length > 0 && received_length <= expected_length;
  uint64_t loaded =
      received_length >= 0 ? static_cast<uint64_t>(received_length) : 0;
  uint64_t total =
      length_computable ? static_cast<uint64_t>(expected_length) : 0;

  std::optional<scheduler::TaskAttributionTracker::TaskScope>
      task_attribution_scope = MaybeCreateTaskAttributionScope();
  ExecutionContext* context = GetExecutionContext();
  probe::AsyncTask async_task(
      context, &async_task_context_,
      type == event_type_names::kLoadend ? nullptr : "progress", async_);
  progress_event_throttle_->DispatchProgressEvent(type, length_computable,
                                                  loaded, total);
}

void XMLHttpRequest::DispatchProgressEventFromSnapshot(
    const AtomicString& type) {
  DispatchProgressEvent(type, received_length_,
                        response_.ExpectedContentLength());
}

void XMLHttpRequest::HandleNetworkError() {
  DVLOG(1) << this << " handleNetworkError()";

  InternalAbort();

  HandleRequestError(DOMExceptionCode::kNetworkError, event_type_names::kError);
}

void XMLHttpRequest::HandleDidCancel() {
  DVLOG(1) << this << " handleDidCancel()";

  InternalAbort();

  pending_abort_event_ = PostCancellableTask(
      *GetExecutionContext()->GetTaskRunner(TaskType::kNetworking), FROM_HERE,
      WTF::BindOnce(&XMLHttpRequest::HandleRequestError, WrapPersistent(this),
                    DOMExceptionCode::kAbortError, event_type_names::kAbort));
}

void XMLHttpRequest::HandleRequestError(DOMExceptionCode exception_code,
                                        const AtomicString& type) {
  DVLOG(1) << this << " handleRequestError()";

  probe::DidFinishXHR(GetExecutionContext(), this);

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

  DispatchProgressEvent(type, /*received_length=*/0, /*expected_length=*/0);
  DispatchProgressEvent(event_type_names::kLoadend, /*received_length=*/0,
                        /*expected_length=*/0);

  parent_task_ = nullptr;
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

  mime_type_override_ = AtomicString("application/octet-stream");
  if (!ParsedContentType(mime_type).IsValid()) {
    return;
  }

  if (!net::ExtractMimeTypeFromMediaType(mime_type.Utf8(),
                                         /*accept_comma_separated=*/false)
           .has_value()) {
    return;
  }

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

  // "5. Terminate these steps if (|name|, |value|) is a forbidden request
  //      header."
  // No script (privileged or not) can set unsafe headers.
  if (cors::IsForbiddenRequestHeader(name, value)) {
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

void XMLHttpRequest::setPrivateToken(const PrivateToken* trust_token,
                                     ExceptionState& exception_state) {
  // These precondition checks are copied from |setRequestHeader|.
  if (state_ != kOpened || send_flag_) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "The object's state must be OPENED.");
    return;
  }

  auto params = network::mojom::blink::TrustTokenParams::New();
  if (!ConvertTrustTokenToMojomAndCheckPermissions(
          *trust_token, GetPSTFeatures(*GetExecutionContext()),
          &exception_state, params.get())) {
    DCHECK(exception_state.HadException());
    return;
  }

  trust_token_params_ = std::move(params);
}

void XMLHttpRequest::setAttributionReporting(
    const AttributionReportingRequestOptions* options,
    ExceptionState& exception_state) {
  // These precondition checks are copied from |setRequestHeader|.
  if (state_ != kOpened || send_flag_) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "The object's state must be OPENED.");
    return;
  }

  attribution_reporting_eligibility_ =
      ConvertAttributionReportingRequestOptionsToMojom(
          *options, *GetExecutionContext(), exception_state);
}

bool XMLHttpRequest::HasContentTypeRequestHeader() const {
  return request_headers_.Find(http_names::kContentType) !=
         request_headers_.end();
}

String XMLHttpRequest::getAllResponseHeaders() const {
  if (state_ < kHeadersReceived || error_)
    return "";

  StringBuilder string_builder;

  HTTPHeaderSet access_control_expose_header_set =
      cors::ExtractCorsExposedHeaderNamesList(
          with_credentials_ ? network::mojom::CredentialsMode::kInclude
                            : network::mojom::CredentialsMode::kSameOrigin,
          response_);

  // "Let |headers| be the result of sorting |initialHeaders| in ascending
  // order, with |a| being less than |b| if |a|’s name is legacy-uppercased-byte
  // less than |b|’s name."
  Vector<std::pair<String, String>> headers;
  // Although we omit some headers in |response_.HttpHeaderFields()| below,
  // we pre-allocate the buffer for performance.
  headers.ReserveInitialCapacity(response_.HttpHeaderFields().size());
  for (auto it = response_.HttpHeaderFields().begin();
       it != response_.HttpHeaderFields().end(); ++it) {
    // Hide any headers whose name is a forbidden response-header name.
    // This is required for all kinds of filtered responses.
    if (FetchUtils::IsForbiddenResponseHeaderName(it->key)) {
      continue;
    }

    if (response_.GetType() == network::mojom::FetchResponseType::kCors &&
        !cors::IsCorsSafelistedResponseHeader(it->key) &&
        access_control_expose_header_set.find(it->key.Ascii()) ==
            access_control_expose_header_set.end()) {
      continue;
    }

    headers.push_back(std::make_pair(it->key.UpperASCII(), it->value));
  }
  std::sort(headers.begin(), headers.end(),
            [](const std::pair<String, String>& x,
               const std::pair<String, String>& y) {
              return CodeUnitCompareLessThan(x.first, y.first);
            });
  for (const auto& header : headers) {
    string_builder.Append(header.first.LowerASCII());
    string_builder.Append(':');
    string_builder.Append(' ');
    string_builder.Append(header.second);
    string_builder.Append('\r');
    string_builder.Append('\n');
  }

  return string_builder.ToString();
}

const AtomicString& XMLHttpRequest::getResponseHeader(
    const AtomicString& name) const {
  if (state_ < kHeadersReceived || error_)
    return g_null_atom;

  if (FetchUtils::IsForbiddenResponseHeaderName(name)) {
    LogConsoleError(GetExecutionContext(),
                    "Refused to get unsafe header \"" + name + "\"");
    return g_null_atom;
  }

  HTTPHeaderSet access_control_expose_header_set =
      cors::ExtractCorsExposedHeaderNamesList(
          with_credentials_ ? network::mojom::CredentialsMode::kInclude
                            : network::mojom::CredentialsMode::kSameOrigin,
          response_);

  if (response_.GetType() == network::mojom::FetchResponseType::kCors &&
      !cors::IsCorsSafelistedResponseHeader(name) &&
      !base::Contains(access_control_expose_header_set, name.Ascii())) {
    LogConsoleError(GetExecutionContext(),
                    "Refused to get unsafe header \"" + name + "\"");
    return g_null_atom;
  }
  return response_.HttpHeaderField(name);
}

AtomicString XMLHttpRequest::FinalResponseMIMETypeInternal() const {
  std::optional<std::string> overridden_type =
      net::ExtractMimeTypeFromMediaType(mime_type_override_.Utf8(),
                                        /*accept_comma_separated=*/false);
  if (overridden_type.has_value()) {
    return AtomicString::FromUTF8(overridden_type->c_str());
  }

  if (response_.IsHTTP()) {
    AtomicString header = response_.HttpHeaderField(http_names::kContentType);
    std::optional<std::string> extracted_type =
        net::ExtractMimeTypeFromMediaType(header.Utf8(),
                                          /*accept_comma_separated=*/true);
    if (extracted_type.has_value()) {
      return AtomicString::FromUTF8(extracted_type->c_str());
    }

    return g_empty_atom;
  }

  return response_.MimeType();
}

// https://xhr.spec.whatwg.org/#response-body
AtomicString XMLHttpRequest::GetResponseMIMEType() const {
  AtomicString final_type = FinalResponseMIMETypeInternal();
  if (!final_type.empty())
    return final_type;

  return AtomicString("text/xml");
}

// https://xhr.spec.whatwg.org/#final-charset
WTF::TextEncoding XMLHttpRequest::FinalResponseCharset() const {
  // 1. Let label be null. [spec text]
  //
  // 2. If response MIME type's parameters["charset"] exists, then set label to
  // it. [spec text]
  String label = response_.TextEncodingName();

  // 3. If override MIME type's parameters["charset"] exists, then set label to
  // it. [spec text]
  String override_response_charset =
      ExtractCharsetFromMediaType(mime_type_override_);
  if (!override_response_charset.empty())
    label = override_response_charset;

  // 4. If label is null, then return null. [spec text]
  //
  // 5. Let encoding be the result of getting an encoding from label. [spec
  // text]
  //
  // 6. If encoding is failure, then return null. [spec text]
  //
  // 7. Return encoding. [spec text]
  //
  // We rely on WTF::TextEncoding() to return invalid TextEncoding for
  // null, empty, or invalid/unsupported |label|.
  return WTF::TextEncoding(label);
}

void XMLHttpRequest::UpdateContentTypeAndCharset(
    const AtomicString& default_content_type,
    const String& charset) {
  // http://xhr.spec.whatwg.org/#the-send()-method step 4's concilliation of
  // "charset=" in any author-provided Content-Type: request header.
  String content_type = request_headers_.Get(http_names::kContentType);
  if (content_type.IsNull()) {
    SetRequestHeaderInternal(http_names::kContentType, default_content_type);
    return;
  }
  String original_content_type = content_type;
  ReplaceCharsetInMediaType(content_type, charset);
  request_headers_.Set(http_names::kContentType, AtomicString(content_type));

  if (original_content_type != content_type) {
    UseCounter::Count(GetExecutionContext(), WebFeature::kReplaceCharsetInXHR);
    if (!EqualIgnoringASCIICase(original_content_type, content_type)) {
      UseCounter::Count(GetExecutionContext(),
                        WebFeature::kReplaceCharsetInXHRIgnoringCase);
    }
  }
}

bool XMLHttpRequest::ResponseIsXML() const {
  return MIMETypeRegistry::IsXMLMIMEType(GetResponseMIMEType());
}

bool XMLHttpRequest::ResponseIsHTML() const {
  return EqualIgnoringASCIICase(FinalResponseMIMETypeInternal(), "text/html");
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

void XMLHttpRequest::DidFail(uint64_t, const ResourceError& error) {
  DVLOG(1) << this << " didFail()";

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

void XMLHttpRequest::DidFailRedirectCheck(uint64_t) {
  DVLOG(1) << this << " didFailRedirectCheck()";

  HandleNetworkError();
}

void XMLHttpRequest::DidFinishLoading(uint64_t identifier) {
  DVLOG(1) << this << " didFinishLoading(" << identifier << ")";

  if (error_)
    return;

  if (state_ < kHeadersReceived)
    ChangeState(kHeadersReceived);

  if (downloading_to_blob_ &&
      response_type_code_ != V8XMLHttpRequestResponseType::Enum::kBlob &&
      response_blob_) {
    // In this case, we have sent the request with DownloadToBlob true,
    // but the user changed the response type after that. Hence we need to
    // read the response data and provide it to this object.
    blob_loader_ = MakeGarbageCollected<BlobLoader>(
        this, response_blob_->GetBlobDataHandle());
  } else {
    DidFinishLoadingInternal();
  }
}

void XMLHttpRequest::DidFinishLoadingInternal() {
  if (response_document_parser_) {
    response_document_parser_->Finish();
    // The remaining logic lives in `XMLHttpRequest::NotifyParserStopped()`
    // which is called by `DocumentParser::Finish()` synchronously or
    // asynchronously.
    return;
  }

  if (decoder_) {
    if (!response_text_overflow_) {
      auto text = decoder_->Flush();
      if (response_text_.DoesAppendCauseOverflow(text.length())) {
        response_text_overflow_ = true;
        response_text_.Clear();
      } else {
        response_text_.Append(text);
      }
    }
    ReportMemoryUsageToV8();
  }

  ClearVariablesForLoading();
  EndLoading();
}

void XMLHttpRequest::DidFinishLoadingFromBlob() {
  DVLOG(1) << this << " didFinishLoadingFromBlob";

  DidFinishLoadingInternal();
}

void XMLHttpRequest::DidFailLoadingFromBlob() {
  DVLOG(1) << this << " didFailLoadingFromBlob()";

  if (error_)
    return;
  HandleNetworkError();
}

void XMLHttpRequest::NotifyParserStopped() {
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
  probe::DidFinishXHR(GetExecutionContext(), this);

  if (loader_) {
    // Set |m_error| in order to suppress the cancel notification (see
    // XMLHttpRequest::didFail).
    base::AutoReset<bool> scope(&error_, true);
    loader_.Release()->Cancel();
  }

  send_flag_ = false;
  ChangeState(kDone);

  if (auto* window = DynamicTo<LocalDOMWindow>(GetExecutionContext())) {
    LocalFrame* frame = window->GetFrame();
    if (frame && network::IsSuccessfulStatus(status()))
      frame->GetPage()->GetChromeClient().AjaxSucceeded(frame);
  }

  parent_task_ = nullptr;
}

void XMLHttpRequest::DidSendData(uint64_t bytes_sent,
                                 uint64_t total_bytes_to_be_sent) {
  DVLOG(1) << this << " didSendData(" << bytes_sent << ", "
           << total_bytes_to_be_sent << ")";
  if (!upload_)
    return;

  if (upload_events_allowed_)
    upload_->DispatchProgressEvent(bytes_sent, total_bytes_to_be_sent);

  if (bytes_sent == total_bytes_to_be_sent && !upload_complete_) {
    upload_complete_ = true;
    if (upload_events_allowed_) {
      upload_->DispatchEventAndLoadEnd(event_type_names::kLoad, true,
                                       bytes_sent, total_bytes_to_be_sent);
    }
  }
}

void XMLHttpRequest::DidReceiveResponse(uint64_t identifier,
                                        const ResourceResponse& response) {
  // TODO(yhirano): Remove this CHECK: see https://crbug.com/570946.
  CHECK(&response);

  DVLOG(1) << this << " didReceiveResponse(" << identifier << ")";
  response_ = response;
}

void XMLHttpRequest::ParseDocumentChunk(base::span<const uint8_t> data) {
  if (!response_document_parser_) {
    DCHECK(!response_document_);
    InitResponseDocument();
    if (!response_document_)
      return;

    response_document_parser_ =
        response_document_->ImplicitOpen(kAllowDeferredParsing);
    response_document_parser_->AddClient(this);
  }
  DCHECK(response_document_parser_);

  if (response_document_parser_->NeedsDecoder())
    response_document_parser_->SetDecoder(CreateDecoder());

  response_document_parser_->AppendBytes(data);
}

std::unique_ptr<TextResourceDecoder> XMLHttpRequest::CreateDecoder() const {
  if (response_type_code_ == V8XMLHttpRequestResponseType::Enum::kJson) {
    return std::make_unique<TextResourceDecoder>(TextResourceDecoderOptions(
        TextResourceDecoderOptions::CreateUTF8Decode()));
  }

  WTF::TextEncoding final_response_charset = FinalResponseCharset();
  if (final_response_charset.IsValid()) {
    // If the final charset is given and valid, use the charset without
    // sniffing the content.
    return std::make_unique<TextResourceDecoder>(TextResourceDecoderOptions(
        TextResourceDecoderOptions::kPlainTextContent, final_response_charset));
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
        return std::make_unique<TextResourceDecoder>(decoder_options_for_xml);
      [[fallthrough]];
    case V8XMLHttpRequestResponseType::Enum::kText:
      return std::make_unique<TextResourceDecoder>(TextResourceDecoderOptions(
          TextResourceDecoderOptions::kPlainTextContent, UTF8Encoding()));

    case V8XMLHttpRequestResponseType::Enum::kDocument:
      if (ResponseIsHTML()) {
        return std::make_unique<TextResourceDecoder>(TextResourceDecoderOptions(
            TextResourceDecoderOptions::kHTMLContent, UTF8Encoding()));
      }
      return std::make_unique<TextResourceDecoder>(decoder_options_for_xml);
    case V8XMLHttpRequestResponseType::Enum::kJson:
    case V8XMLHttpRequestResponseType::Enum::kBlob:
    case V8XMLHttpRequestResponseType::Enum::kArraybuffer:
      NOTREACHED_IN_MIGRATION();
      break;
  }
  NOTREACHED_IN_MIGRATION();
  return nullptr;
}

void XMLHttpRequest::DidReceiveData(base::span<const char> data) {
  if (error_)
    return;

  DCHECK(!downloading_to_blob_ || blob_loader_);

  if (state_ < kHeadersReceived)
    ChangeState(kHeadersReceived);

  // We need to check for |m_error| again, because |changeState| may trigger
  // readystatechange, and user javascript can cause |abort()|.
  if (error_)
    return;

  if (data.empty()) {
    return;
  }

  if (response_type_code_ == V8XMLHttpRequestResponseType::Enum::kDocument &&
      ResponseIsHTML()) {
    ParseDocumentChunk(base::as_bytes(data));
  } else if (response_type_code_ == kResponseTypeDefault ||
             response_type_code_ == V8XMLHttpRequestResponseType::Enum::kText ||
             response_type_code_ == V8XMLHttpRequestResponseType::Enum::kJson ||
             response_type_code_ ==
                 V8XMLHttpRequestResponseType::Enum::kDocument) {
    if (!decoder_)
      decoder_ = CreateDecoder();

    if (!response_text_overflow_) {
      if (response_text_.DoesAppendCauseOverflow(
              base::checked_cast<unsigned>(data.size()))) {
        response_text_overflow_ = true;
        response_text_.Clear();
      } else {
        response_text_.Append(decoder_->Decode(data));
      }
      ReportMemoryUsageToV8();
    }
  } else if (response_type_code_ ==
                 V8XMLHttpRequestResponseType::Enum::kArraybuffer ||
             response_type_code_ == V8XMLHttpRequestResponseType::Enum::kBlob) {
    // Buffer binary data.
    if (!binary_response_builder_)
      binary_response_builder_ = SharedBuffer::Create();
    binary_response_builder_->Append(data);
    ReportMemoryUsageToV8();
  }

  if (blob_loader_) {
    // In this case, the data is provided by m_blobLoader. As progress
    // events are already fired, we should return here.
    return;
  }
  TrackProgress(data.size());
}

void XMLHttpRequest::DidDownloadData(uint64_t data_length) {
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
    String mime_type = GetResponseMIMEType().LowerASCII();
    if (blob->GetType() != mime_type) {
      auto blob_size = blob->size();
      auto blob_data = std::make_unique<BlobData>();
      blob_data->SetContentType(mime_type);
      blob_data->AppendBlob(std::move(blob), 0, blob_size);
      response_blob_ = MakeGarbageCollected<Blob>(
          BlobDataHandle::Create(std::move(blob_data), blob_size));
    } else {
      response_blob_ = MakeGarbageCollected<Blob>(std::move(blob));
    }
  }
}

void XMLHttpRequest::HandleDidTimeout() {
  DVLOG(1) << this << " handleDidTimeout()";

  InternalAbort();

  HandleRequestError(DOMExceptionCode::kTimeoutError,
                     event_type_names::kTimeout);
}

void XMLHttpRequest::ContextDestroyed() {
  Dispose();

  // In case we are in the middle of send() function, unset the send flag to
  // stop the operation.
  send_flag_ = false;
}

bool XMLHttpRequest::HasPendingActivity() const {
  // Neither this object nor the JavaScript wrapper should be deleted while
  // a request is in progress because we need to keep the listeners alive,
  // and they are referenced by the JavaScript wrapper.
  // `loader_` is non-null while request is active and ThreadableLoaderClient
  // callbacks may be called, and `response_document_parser_` is non-null while
  // DocumentParserClient callbacks may be called.
  // TODO(crbug.com/1486065): I believe we actually don't need
  // `response_document_parser_` condition.
  return loader_ || response_document_parser_;
}

const AtomicString& XMLHttpRequest::InterfaceName() const {
  return event_target_names::kXMLHttpRequest;
}

ExecutionContext* XMLHttpRequest::GetExecutionContext() const {
  return ExecutionContextLifecycleObserver::GetExecutionContext();
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

  // Text
  const size_t response_text_size =
      response_text_.Capacity() *
      (response_text_.Is8Bit() ? sizeof(LChar) : sizeof(UChar));
  diff += static_cast<int64_t>(response_text_size) -
          static_cast<int64_t>(response_text_last_reported_size_);
  response_text_last_reported_size_ = response_text_size;

  if (diff) {
    external_memory_accounter_.Update(v8::Isolate::GetCurrent(), diff);
  }
}

void XMLHttpRequest::Trace(Visitor* visitor) const {
  visitor->Trace(response_blob_);
  visitor->Trace(loader_);
  visitor->Trace(response_document_);
  visitor->Trace(response_document_parser_);
  visitor->Trace(response_array_buffer_);
  visitor->Trace(progress_event_throttle_);
  visitor->Trace(world_);
  visitor->Trace(upload_);
  visitor->Trace(blob_loader_);
  visitor->Trace(parent_task_);
  XMLHttpRequestEventTarget::Trace(visitor);
  ThreadableLoaderClient::Trace(visitor);
  DocumentParserClient::Trace(visitor);
  ExecutionContextLifecycleObserver::Trace(visitor);
}

bool XMLHttpRequest::HasRequestHeaderForTesting(AtomicString name) const {
  return request_headers_.Contains(name);
}

std::optional<scheduler::TaskAttributionTracker::TaskScope>
XMLHttpRequest::MaybeCreateTaskAttributionScope() {
  if (!parent_task_ || !GetExecutionContext() ||
      GetExecutionContext()->IsContextDestroyed()) {
    return std::nullopt;
  }
  // `parent_task_` being non-null implies that task tracking is enabled and
  // this object is associated with the main world.
  auto* script_state = ToScriptStateForMainWorld(GetExecutionContext());
  CHECK(script_state);
  auto* tracker =
      scheduler::TaskAttributionTracker::From(script_state->GetIsolate());
  CHECK(tracker);

  // Don't create a new (nested) task scope if we're still in the parent task,
  // otherwise we risk clobbering other propagated task state.
  //
  // TODO(crbug.com/1439971): Make this safe to do or move the logic into the
  // task attribution implementation.
  if (tracker->RunningTask() == parent_task_.Get()) {
    return std::nullopt;
  }
  return tracker->CreateTaskScope(
      script_state, parent_task_,
      scheduler::TaskAttributionTracker::TaskScopeType::kXMLHttpRequest);
}

std::ostream& operator<<(std::ostream& ostream, const XMLHttpRequest* xhr) {
  return ostream << "XMLHttpRequest " << static_cast<const void*>(xhr);
}

}  // namespace blink
