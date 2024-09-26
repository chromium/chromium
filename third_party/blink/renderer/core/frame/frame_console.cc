/*
 * Copyright (C) 2013 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/core/frame/frame_console.h"

#include <memory>

#include "third_party/blink/public/common/features.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/core/inspector/console_message_storage.h"
#include "third_party/blink/renderer/core/inspector/main_thread_debugger.h"
#include "third_party/blink/renderer/core/loader/document_loader.h"
#include "third_party/blink/renderer/core/page/chrome_client.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/platform/bindings/source_location.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_error.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_response.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace blink {

FrameConsole::FrameConsole(LocalFrame& frame) : frame_(&frame) {}

void FrameConsole::AddMessage(ConsoleMessage* console_message,
                              bool discard_duplicates) {
  if (AddMessageToStorage(console_message, discard_duplicates)) {
    ReportMessageToClient(
        console_message->GetSource(), console_message->GetLevel(),
        console_message->Message(), console_message->Location());
  }
}

bool FrameConsole::AddMessageToStorage(ConsoleMessage* console_message,
                                       bool discard_duplicates) {
  if (!frame_->DomWindow())
    return false;
  return frame_->GetPage()->GetConsoleMessageStorage().AddConsoleMessage(
      frame_->DomWindow(), console_message, discard_duplicates);
}

void FrameConsole::ReportMessageToClient(
    mojom::blink::ConsoleMessageSource source,
    mojom::blink::ConsoleMessageLevel level,
    const String& message,
    SourceLocation* location) {
  if (source == mojom::blink::ConsoleMessageSource::kNetwork)
    return;

  String url = location->Url();
  String stack_trace;
  if (source == mojom::blink::ConsoleMessageSource::kConsoleApi) {
    if (!frame_->GetPage())
      return;
    if (frame_->GetChromeClient()
            .ShouldReportDetailedMessageForSourceAndSeverity(*frame_, level,
                                                             url)) {
      std::unique_ptr<SourceLocation> full_location =
          SourceLocation::CaptureWithFullStackTrace();
      if (!full_location->IsUnknown())
        stack_trace = full_location->ToString();
    }
  } else {
    if (!location->IsUnknown() &&
        frame_->GetChromeClient()
            .ShouldReportDetailedMessageForSourceAndSeverity(*frame_, level,
                                                             url))
      stack_trace = location->ToString();
  }

  frame_->GetChromeClient().AddMessageToConsole(
      frame_, source, level, message, location->LineNumber(), url, stack_trace);
}

void FrameConsole::ReportResourceResponseReceived(
    DocumentLoader* loader,
    uint64_t request_identifier,
    const ResourceResponse& response) {
  if (!loader)
    return;
  if (response.HttpStatusCode() < 400)
    return;
  String message =
      "Failed to load resource: the server responded with a status of " +
      String::Number(response.HttpStatusCode()) + " (" +
      response.HttpStatusText() + ')';
  auto* console_message = MakeGarbageCollected<ConsoleMessage>(
      mojom::blink::ConsoleMessageSource::kNetwork,
      mojom::blink::ConsoleMessageLevel::kError, message,
      response.CurrentRequestUrl().GetString(), loader, request_identifier);
  AddMessage(console_message);
}

void FrameConsole::DidFailLoading(DocumentLoader* loader,
                                  uint64_t request_identifier,
                                  const ResourceError& error) {
  // Report failures only.
  if (error.IsCancellation() || error.IsUnactionableTrustTokensStatus())
    return;

  if (error.WasBlockedByORB()) {
    // ORB loading errors are reported from the network service directly to
    // DevTools (CorsURLLoader::ReportOrbErrorToDevTools).
    return;
  }

  StringBuilder message;
  message.Append("Failed to load resource");
  if (!error.LocalizedDescription().empty()) {
    message.Append(": ");
    message.Append(error.LocalizedDescription());
  }
  auto* console_message = MakeGarbageCollected<ConsoleMessage>(
      mojom::blink::ConsoleMessageSource::kNetwork,
      mojom::blink::ConsoleMessageLevel::kError, message.ToString(),
      error.FailingURL(), loader, request_identifier);
  if (error.CorsErrorStatus()) {
    console_message->SetCategory(mojom::blink::ConsoleMessageCategory::Cors);
  }
  AddMessageToStorage(console_message);
}

void FrameConsole::Trace(Visitor* visitor) const {
  visitor->Trace(frame_);
}

}  // namespace blink
