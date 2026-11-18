// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/extensions_web_local_frame_observer.h"

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/strings/string_split.h"
#include "content/public/renderer/render_frame.h"
#include "extensions/common/extension_urls.h"
#include "extensions/common/logging_constants.h"
#include "extensions/common/stack_frame.h"
#include "extensions/renderer/extension_frame_helper.h"
#include "third_party/blink/public/common/logging/logging_utils.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/web/web_console_message.h"
#include "third_party/blink/public/web/web_frame_widget.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/skia/include/core/SkColor.h"

namespace extensions {

namespace {

// The delimiter for a stack trace provided by WebKit.
const char16_t kStackFrameDelimiter[] = u"\n    at ";

// Get a stack trace from a WebKit console message.
// There are three possible scenarios:
// 1. WebKit gives us a stack trace in |stack_trace|.
// 2. The stack trace is embedded in the error |message| by an internal
//    script. This will be more useful than |stack_trace|, since |stack_trace|
//    will include the internal bindings trace, instead of a developer's code.
// 3. No stack trace is included. In this case, we should mock one up from
//    the given line number and source.
// |message| will be populated with the error message only (i.e., will not
// include any stack trace).
StackTrace GetStackTraceFromMessage(std::u16string* message,
                                    const std::u16string& source,
                                    const std::u16string& stack_trace,
                                    uint32_t line_number) {
  StackTrace result;
  std::vector<std::u16string> pieces;
  size_t index = 0;

  if (message->find(kStackFrameDelimiter) != std::u16string::npos) {
    pieces = base::SplitStringUsingSubstr(*message, kStackFrameDelimiter,
                                          base::TRIM_WHITESPACE,
                                          base::SPLIT_WANT_ALL);
    *message = pieces[0];
    index = 1;
  } else if (!stack_trace.empty()) {
    pieces = base::SplitStringUsingSubstr(stack_trace, kStackFrameDelimiter,
                                          base::TRIM_WHITESPACE,
                                          base::SPLIT_WANT_ALL);
  }

  // If we got a stack trace, parse each frame from the text.
  if (index < pieces.size()) {
    for (; index < pieces.size(); ++index) {
      std::unique_ptr<StackFrame> frame =
          StackFrame::CreateFromText(pieces[index]);
      if (frame.get()) {
        result.push_back(*frame);
      }
    }
  }

  if (result.empty()) {  // If we don't have a stack trace, mock one up.
    result.push_back(StackFrame(line_number,
                                1u,  // column number
                                source,
                                std::u16string() /* no function name */));
  }

  return result;
}

}  // namespace

ExtensionsWebLocalFrameObserver::ExtensionsWebLocalFrameObserver(
    blink::WebLocalFrame* web_local_frame,
    service_manager::BinderRegistry* registry)
    : blink::WebLocalFrameObserver(web_local_frame) {
  registry->AddInterface(base::BindRepeating(
      &ExtensionsWebLocalFrameObserver::BindAppWindowReceiver,
      weak_ptr_factory_.GetWeakPtr()));
}

ExtensionsWebLocalFrameObserver::~ExtensionsWebLocalFrameObserver() = default;

void ExtensionsWebLocalFrameObserver::BindAppWindowReceiver(
    mojo::PendingReceiver<mojom::AppWindow> receiver) {
  receivers_.Add(this, std::move(receiver));
}

void ExtensionsWebLocalFrameObserver::SetVisuallyDeemphasized(
    bool deemphasized) {
  // TODO(danakj): This mojo API should be a MainFrame-only interface and object
  // rather than an every-frame interface and object.
  DCHECK(!GetWebLocalFrame()->Parent());

  if (webview_visually_deemphasized_ == deemphasized) {
    return;
  }

  webview_visually_deemphasized_ = deemphasized;

  SkColor color =
      deemphasized ? SkColorSetARGB(178, 0, 0, 0) : SK_ColorTRANSPARENT;
  GetWebLocalFrame()->FrameWidget()->SetMainFrameOverlayColor(color);
}

void ExtensionsWebLocalFrameObserver::DidAddMessageToConsole(
    const blink::WebConsoleMessage& message,
    const blink::WebString& source_name,
    unsigned source_line,
    const blink::WebString& stack_trace_string) {
  if (blink::ConsoleMessageLevelToLogSeverity(message.level) <
      static_cast<int32_t>(extension_misc::kMinimumSeverityToReportError)) {
    // We don't report certain low-severity errors.
    return;
  }

  std::u16string source = source_name.Utf16();
  if (!IsSourceFromAnExtension(source)) {
    return;
  }

  std::u16string trimmed_message = message.text.Utf16();
  StackTrace stack_trace = GetStackTraceFromMessage(
      &trimmed_message, source, stack_trace_string.Utf16(), source_line);
  ExtensionFrameHelper::Get(
      content::RenderFrame::FromWebFrame(GetWebLocalFrame()))
      ->GetLocalFrameHost()
      ->DetailedConsoleMessageAdded(trimmed_message, source, stack_trace,
                                    message.level);
}

void ExtensionsWebLocalFrameObserver::OnFrameDetached() {
  delete this;
}

}  // namespace extensions
