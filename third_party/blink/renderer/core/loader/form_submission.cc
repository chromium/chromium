/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/core/loader/form_submission.h"

#include "services/network/public/cpp/is_potentially_trustworthy.h"
#include "third_party/blink/public/common/security_context/insecure_request_policy.h"
#include "third_party/blink/public/mojom/frame/frame.mojom-blink.h"
#include "third_party/blink/public/mojom/security_context/insecure_request_policy.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/capture_source_location.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/policy_container.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/frame/web_feature.h"
#include "third_party/blink/renderer/core/html/forms/form_data.h"
#include "third_party/blink/renderer/core/html/forms/html_form_control_element.h"
#include "third_party/blink/renderer/core/html/forms/html_form_element.h"
#include "third_party/blink/renderer/core/html/parser/html_parser_idioms.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/loader/frame_load_request.h"
#include "third_party/blink/renderer/core/loader/frame_loader.h"
#include "third_party/blink/renderer/platform/bindings/source_location.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_request.h"
#include "third_party/blink/renderer/platform/network/encoded_form_data.h"
#include "third_party/blink/renderer/platform/network/form_data_encoder.h"
#include "third_party/blink/renderer/platform/network/http_names.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"
#include "third_party/blink/renderer/platform/wtf/text/text_encoding.h"

namespace blink {

static int64_t GenerateFormDataIdentifier() {
  // Initialize to the current time to reduce the likelihood of generating
  // identifiers that overlap with those from past/future browser sessions.
  static int64_t next_identifier =
      (base::Time::Now() - base::Time::UnixEpoch()).InMicroseconds();
  return ++next_identifier;
}

static void AppendMailtoPostFormDataToURL(KURL& url,
                                          const EncodedFormData& data,
                                          const String& encoding_type) {
  String body = data.FlattenToString();

  if (EqualIgnoringASCIICase(encoding_type, "text/plain")) {
    // Convention seems to be to decode, and s/&/\r\n/. Also, spaces are encoded
    // as %20.
    body = DecodeURLEscapeSequences(
        String(body.Replace('&', "\r\n").Replace('+', ' ') + "\r\n"),
        DecodeURLMode::kUTF8OrIsomorphic);
  }

  Vector<char> body_data;
  body_data.AppendSpan(base::span_from_cstring("body="));
  FormDataEncoder::EncodeStringAsFormData(body_data, body.Utf8(),
                                          FormDataEncoder::kNormalizeCRLF);
  body = String(body_data.data(), body_data.size()).Replace('+', "%20");

  StringBuilder query;
  query.Append(url.Query());
  if (!query.empty())
    query.Append('&');
  query.Append(body);
  url.SetQuery(query.ToString());
}

void FormSubmission::Attributes::ParseAction(const String& action) {
  // m_action cannot be converted to KURL (bug https://crbug.com/388664)
  action_ = StripLeadingAndTrailingHTMLSpaces(action);
}

AtomicString FormSubmission::Attributes::ParseEncodingType(const String& type) {
  if (EqualIgnoringASCIICase(type, "multipart/form-data"))
    return AtomicString("multipart/form-data");
  if (EqualIgnoringASCIICase(type, "text/plain"))
    return AtomicString("text/plain");
  return AtomicString("application/x-www-form-urlencoded");
}

void FormSubmission::Attributes::UpdateEncodingType(const String& type) {
  encoding_type_ = ParseEncodingType(type);
  is_multi_part_form_ = (encoding_type_ == "multipart/form-data");
}

FormSubmission::SubmitMethod FormSubmission::Attributes::ParseMethodType(
    const String& type) {
  if (EqualIgnoringASCIICase(type, "post"))
    return FormSubmission::kPostMethod;
  if (EqualIgnoringASCIICase(type, "dialog"))
    return FormSubmission::kDialogMethod;
  return FormSubmission::kGetMethod;
}

void FormSubmission::Attributes::UpdateMethodType(const String& type) {
  method_ = ParseMethodType(type);
}

String FormSubmission::Attributes::MethodString(SubmitMethod method) {
  switch (method) {
    case kGetMethod:
      return "get";
    case kPostMethod:
      return "post";
    case kDialogMethod:
      return "dialog";
  }
  NOTREACHED_IN_MIGRATION();
  return g_empty_string;
}

void FormSubmission::Attributes::CopyFrom(const Attributes& other) {
  method_ = other.method_;
  is_multi_part_form_ = other.is_multi_part_form_;

  action_ = other.action_;
  target_ = other.target_;
  encoding_type_ = other.encoding_type_;
  accept_charset_ = other.accept_charset_;
}

inline FormSubmission::FormSubmission(
    SubmitMethod method,
    const KURL& action,
    const AtomicString& target,
    const AtomicString& content_type,
    Element* submitter,
    scoped_refptr<EncodedFormData> data,
    const Event* event,
    NavigationPolicy navigation_policy,
    mojom::blink::TriggeringEventInfo triggering_event_info,
    ClientNavigationReason reason,
    std::unique_ptr<ResourceRequest> resource_request,
    Frame* target_frame,
    WebFrameLoadType load_type,
    LocalDOMWindow* origin_window,
    const LocalFrameToken& initiator_frame_token,
    bool has_rel_opener,
    std::unique_ptr<SourceLocation> source_location,
    mojo::PendingRemote<mojom::blink::NavigationStateKeepAliveHandle>
        initiator_navigation_state_keep_alive_handle)
    : method_(method),
      action_(action),
      target_(target),
      content_type_(content_type),
      submitter_(submitter),
      form_data_(std::move(data)),
      navigation_policy_(navigation_policy),
      triggering_event_info_(triggering_event_info),
      reason_(reason),
      resource_request_(std::move(resource_request)),
      target_frame_(target_frame),
      load_type_(load_type),
      origin_window_(origin_window),
      initiator_frame_token_(initiator_frame_token),
      has_rel_opener_(has_rel_opener),
      source_location_(std::move(source_location)),
      initiator_navigation_state_keep_alive_handle_(
          std::move(initiator_navigation_state_keep_alive_handle)) {}

inline FormSubmission::FormSubmission(const String& result)
    : method_(kDialogMethod), result_(result) {}

FormSubmission* FormSubmission::Create(HTMLFormElement* form,
                                       const Attributes& attributes,
                                       const Event* event,
                                       HTMLFormControlElement* submit_button) {
  DCHECK(form);

  FormSubmission::Attributes copied_attributes;
  copied_attributes.CopyFrom(attributes);
  if (submit_button) {
    AtomicString attribute_value;
    if (!(attribute_value =
              submit_button->FastGetAttribute(html_names::kFormactionAttr))
             .IsNull())
      copied_attributes.ParseAction(attribute_value);
    if (!(attribute_value =
              submit_button->FastGetAttribute(html_names::kFormenctypeAttr))
             .IsNull())
      copied_attributes.UpdateEncodingType(attribute_value);
    if (!(attribute_value =
              submit_button->FastGetAttribute(html_names::kFormmethodAttr))
             .IsNull())
      copied_attributes.UpdateMethodType(attribute_value);
    if (!(attribute_value =
              submit_button->FastGetAttribute(html_names::kFormtargetAttr))
             .IsNull())
      copied_attributes.SetTarget(attribute_value);
  }

  if (copied_attributes.Method() == kDialogMethod) {
    if (submit_button) {
      return MakeGarbageCollected<FormSubmission>(
          submit_button->ResultForDialogSubmit());
    }
    return MakeGarbageCollected<FormSubmission>("");
  }

  Document& document = form->GetDocument();
  KURL action_url = document.CompleteURL(copied_attributes.Action().empty()
                                             ? document.Url().GetString()
                                             : copied_attributes.Action());

  if ((document.domWindow()->GetSecurityContext().GetInsecureRequestPolicy() &
       mojom::blink::InsecureRequestPolicy::kUpgradeInsecureRequests) !=
          mojom::blink::InsecureRequestPolicy::kLeaveInsecureRequestsAlone &&
      action_url.ProtocolIs("http") &&
      !network::IsUrlPotentiallyTrustworthy(GURL(action_url))) {
    UseCounter::Count(document,
                      WebFeature::kUpgradeInsecureRequestsUpgradedRequestForm);
    action_url.SetProtocol("https");
    if (action_url.Port() == 80)
      action_url.SetPort(443);
  }

  bool is_mailto_form = action_url.ProtocolIs("mailto");
  bool is_multi_part_form = false;
  AtomicString encoding_type = copied_attributes.EncodingType();

  if (copied_attributes.Method() == kPostMethod) {
    is_multi_part_form = copied_attributes.IsMultiPartForm();
    if (is_multi_part_form && is_mailto_form) {
      encoding_type = AtomicString("application/x-www-form-urlencoded");
      is_multi_part_form = false;
    }
  }
  WTF::TextEncoding data_encoding =
      is_mailto_form
          ? UTF8Encoding()
          : FormDataEncoder::EncodingFromAcceptCharset(
                copied_attributes.AcceptCharset(), document.Encoding());
  FormData* dom_form_data = form->ConstructEntryList(
      submit_button, data_encoding.EncodingForFormSubmission());
  DCHECK(dom_form_data);

  scoped_refptr<EncodedFormData> form_data;
  String boundary;

  if (is_multi_part_form) {
    form_data = dom_form_data->EncodeMultiPartFormData();
    boundary = form_data->Boundary().data();
  } else {
    form_data = dom_form_data->EncodeFormData(
        attributes.Method() == kGetMethod
            ? EncodedFormData::kFormURLEncoded
            : EncodedFormData::ParseEncodingType(encoding_type));
    if (copied_attributes.Method() == kPostMethod && is_mailto_form) {
      // Convert the form data into a string that we put into the URL.
      AppendMailtoPostFormDataToURL(action_url, *form_data, encoding_type);
      form_data = EncodedFormData::Create();
    }
  }

  form_data->SetIdentifier(GenerateFormDataIdentifier());
  form_data->SetContainsPasswordData(dom_form_data->ContainsPasswordData());

  if (copied_attributes.Method() != FormSubmission::kPostMethod &&
      !action_url.ProtocolIsJavaScript()) {
    action_url.SetQuery(form_data->FlattenToString());
  }

  std::unique_ptr<ResourceRequest> resource_request =
      std::make_unique<ResourceRequest>(action_url);
  ClientNavigationReason reason = ClientNavigationReason::kFormSubmissionGet;
  if (copied_attributes.Method() == FormSubmission::kPostMethod) {
    reason = ClientNavigationReason::kFormSubmissionPost;
    resource_request->SetHttpMethod(http_names::kPOST);
    resource_request->SetHttpBody(form_data);

    // construct some user headers if necessary
    if (boundary.empty()) {
      resource_request->SetHTTPContentType(encoding_type);
    } else {
      resource_request->SetHTTPContentType(encoding_type +
                                           "; boundary=" + boundary);
    }
  }
  LocalFrame* form_local_frame = form->GetDocument().GetFrame();
  resource_request->SetHasUserGesture(
      LocalFrame::HasTransientUserActivation(form_local_frame));
  resource_request->SetFormSubmission(true);

  mojom::blink::TriggeringEventInfo triggering_event_info;
  if (event) {
    triggering_event_info =
        event->isTrusted()
            ? mojom::blink::TriggeringEventInfo::kFromTrustedEvent
            : mojom::blink::TriggeringEventInfo::kFromUntrustedEvent;
    if (event->UnderlyingEvent())
      event = event->UnderlyingEvent();
  } else {
    triggering_event_info = mojom::blink::TriggeringEventInfo::kNotFromEvent;
  }

  FrameLoadRequest frame_request(form->GetDocument().domWindow(),
                                 *resource_request);
  NavigationPolicy navigation_policy = NavigationPolicyFromEvent(event);
  if (navigation_policy == kNavigationPolicyLinkPreview) {
    return nullptr;
  }
  frame_request.SetNavigationPolicy(navigation_policy);
  frame_request.SetClientNavigationReason(reason);
  if (submit_button) {
    frame_request.SetSourceElement(submit_button);
  } else {
    frame_request.SetSourceElement(form);
  }
  frame_request.SetTriggeringEventInfo(triggering_event_info);
  AtomicString target_or_base_target = frame_request.CleanNavigationTarget(
      copied_attributes.Target().empty() ? document.BaseTarget()
                                         : copied_attributes.Target());

  if (form->HasRel(HTMLFormElement::kNoReferrer)) {
    frame_request.SetNoReferrer();
    frame_request.SetNoOpener();
  }
  if (form->HasRel(HTMLFormElement::kNoOpener) ||
      (EqualIgnoringASCIICase(target_or_base_target, "_blank") &&
       !form->HasRel(HTMLFormElement::kOpener) &&
       form->GetDocument()
           .domWindow()
           ->GetFrame()
           ->GetSettings()
           ->GetTargetBlankImpliesNoOpenerEnabledWillBeRemoved())) {
    frame_request.SetNoOpener();
  }
  if (RuntimeEnabledFeatures::RelOpenerBcgDependencyHintEnabled(
          document.domWindow()) &&
      form->HasRel(HTMLFormElement::kOpener) &&
      !frame_request.GetWindowFeatures().noopener) {
    frame_request.SetExplicitOpener();
  }

  Frame* target_frame =
      form_local_frame->Tree()
          .FindOrCreateFrameForNavigation(frame_request, target_or_base_target)
          .frame;

  // Apply replacement now, before any async steps, as the result may change.
  WebFrameLoadType load_type = WebFrameLoadType::kStandard;
  LocalFrame* target_local_frame = DynamicTo<LocalFrame>(target_frame);
  if (target_local_frame &&
      target_local_frame->NavigationShouldReplaceCurrentHistoryEntry(
          frame_request, load_type)) {
    load_type = WebFrameLoadType::kReplaceCurrentItem;
  }

  return MakeGarbageCollected<FormSubmission>(
      copied_attributes.Method(), action_url, target_or_base_target,
      encoding_type, frame_request.GetSourceElement(), std::move(form_data),
      event, frame_request.GetNavigationPolicy(), triggering_event_info, reason,
      std::move(resource_request), target_frame, load_type,
      form->GetDocument().domWindow(), form_local_frame->GetLocalFrameToken(),
      frame_request.GetWindowFeatures().explicit_opener,
      CaptureSourceLocation(form->GetDocument().domWindow()),
      form_local_frame->IssueKeepAliveHandle());
}

void FormSubmission::Trace(Visitor* visitor) const {
  visitor->Trace(submitter_);
  visitor->Trace(target_frame_);
  visitor->Trace(origin_window_);
}

void FormSubmission::Navigate() {
  FrameLoadRequest frame_request(origin_window_.Get(), *resource_request_);
  frame_request.SetNavigationPolicy(navigation_policy_);
  frame_request.SetClientNavigationReason(reason_);
  frame_request.SetSourceElement(submitter_);
  frame_request.SetTriggeringEventInfo(triggering_event_info_);
  frame_request.SetInitiatorFrameToken(initiator_frame_token_);
  frame_request.SetInitiatorNavigationStateKeepAliveHandle(
      std::move(initiator_navigation_state_keep_alive_handle_));
  frame_request.SetSourceLocation(std::move(source_location_));
  if (has_rel_opener_) {
    frame_request.SetExplicitOpener();
  }

  if (target_frame_ && !target_frame_->GetPage())
    return;

  if (target_frame_)
    target_frame_->Navigate(frame_request, load_type_);
}

}  // namespace blink
