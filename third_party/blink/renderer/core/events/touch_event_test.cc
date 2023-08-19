// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/events/touch_event.h"

#include "base/time/time.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/devtools/console_message.mojom-blink.h"
#include "third_party/blink/renderer/core/frame/frame_console.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/loader/empty_clients.h"
#include "third_party/blink/renderer/core/script/classic_script.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

using testing::ElementsAre;

namespace blink {

class ConsoleCapturingChromeClient : public EmptyChromeClient {
 public:
  ConsoleCapturingChromeClient() : EmptyChromeClient() {}

  // ChromeClient methods:
  void AddMessageToConsole(LocalFrame*,
                           mojom::ConsoleMessageSource message_source,
                           mojom::ConsoleMessageLevel,
                           const String& message,
                           unsigned line_number,
                           const String& source_id,
                           const String& stack_trace) override {
    messages_.push_back(message);
    message_sources_.push_back(message_source);
  }

  // Expose console output.
  const Vector<String>& Messages() { return messages_; }
  const Vector<mojom::ConsoleMessageSource>& MessageSources() {
    return message_sources_;
  }

 private:
  Vector<String> messages_;
  Vector<mojom::ConsoleMessageSource> message_sources_;
};

class TouchEventTest : public PageTestBase {
 public:
  void SetUp() override {
    chrome_client_ = MakeGarbageCollected<ConsoleCapturingChromeClient>();
    SetupPageWithClients(chrome_client_);
    Page::InsertOrdinaryPageForTesting(&GetPage());
  }

  const Vector<String>& Messages() { return chrome_client_->Messages(); }
  const Vector<mojom::ConsoleMessageSource>& MessageSources() {
    return chrome_client_->MessageSources();
  }

  LocalDOMWindow& Window() { return *GetFrame().DomWindow(); }

  TouchEvent* EventWithDispatchType(WebInputEvent::DispatchType dispatch_type) {
    WebTouchEvent web_touch_event(WebInputEvent::Type::kTouchStart, 0,
                                  base::TimeTicks());
    web_touch_event.dispatch_type = dispatch_type;
    return TouchEvent::Create(
        WebCoalescedInputEvent(web_touch_event, ui::LatencyInfo()), nullptr,
        nullptr, nullptr, event_type_names::kTouchstart, &Window(),
        TouchAction::kAuto);
  }

 private:
  Persistent<ConsoleCapturingChromeClient> chrome_client_;
  std::unique_ptr<DummyPageHolder> page_holder_;
};

TEST_F(TouchEventTest,
       PreventDefaultPassiveDueToDocumentLevelScrollerIntervention) {
  TouchEvent* event = EventWithDispatchType(
      WebInputEvent::DispatchType::kListenersNonBlockingPassive);
  event->SetHandlingPassive(Event::PassiveMode::kPassiveForcedDocumentLevel);

  EXPECT_THAT(Messages(), ElementsAre());
  event->preventDefault();
  EXPECT_THAT(
      Messages(),
      ElementsAre("Unable to preventDefault inside passive event listener due "
                  "to target being treated as passive. See "
                  "https://www.chromestatus.com/feature/5093566007214080"));
  EXPECT_THAT(MessageSources(),
              ElementsAre(mojom::ConsoleMessageSource::kIntervention));
}

TEST_F(TouchEventTest, DispatchWithEmptyDocTargetDoesntCrash) {
  String script =
      "var empty_document = new Document();"
      "var touch = new Touch({'identifier': 0, 'target': empty_document});"
      "var touch_event = new TouchEvent('touchmove', {'touches': [touch]});"
      "document.dispatchEvent(touch_event);";

  GetDocument().GetSettings()->SetScriptEnabled(true);
  ClassicScript::CreateUnspecifiedScript(script)->RunScript(
      GetDocument().domWindow());
}

class TouchEventTestNoFrame : public testing::Test {};

TEST_F(TouchEventTestNoFrame, PreventDefaultDoesntRequireFrame) {
  TouchEvent::Create()->preventDefault();
}

}  // namespace blink
