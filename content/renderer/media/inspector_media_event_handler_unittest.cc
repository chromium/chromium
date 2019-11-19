// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>
#include <utility>

#include "content/renderer/media/inspector_media_event_handler.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/web/web_media_inspector.h"

using ::testing::_;

namespace content {

class MockMediaInspectorContext : public blink::MediaInspectorContext {
 public:
  MockMediaInspectorContext() = default;
  virtual ~MockMediaInspectorContext() = default;

  blink::WebString CreatePlayer() override { return "TestPlayer"; }

  void NotifyPlayerEvents(blink::WebString id,
                          blink::InspectorPlayerEvents events) override {
    MockNotifyPlayerEvents(events);
  }

  void SetPlayerProperties(blink::WebString id,
                           blink::InspectorPlayerProperties props) override {
    MockSetPlayerProperties(props);
  }

  MOCK_METHOD1(MockNotifyPlayerEvents, void(blink::InspectorPlayerEvents));
  MOCK_METHOD1(MockSetPlayerProperties, void(blink::InspectorPlayerProperties));
};

class InspectorMediaEventHandlerTest : public testing::Test {
 public:
  InspectorMediaEventHandlerTest() {
    mock_context_ = std::make_unique<MockMediaInspectorContext>();
    handler_ =
        std::make_unique<InspectorMediaEventHandler>(mock_context_.get());
  }

 protected:
  std::unique_ptr<InspectorMediaEventHandler> handler_;
  std::unique_ptr<MockMediaInspectorContext> mock_context_;

  media::MediaLogEvent CreateEvent(media::MediaLogEvent::Type type) {
    media::MediaLogEvent event;
    event.id = 0;
    event.type = type;
    event.time = base::TimeTicks();
    return event;
  }

  media::MediaLogEvent CreatePropChangeEvent(
      std::vector<std::pair<std::string, std::string>> props) {
    auto event = CreateEvent(media::MediaLogEvent::PROPERTY_CHANGE);
    for (auto p : props) {
      event.params.SetString(std::get<0>(p), std::get<1>(p));
    }
    return event;
  }

  media::MediaLogEvent CreateLogEvent(std::string msg) {
    auto event = CreateEvent(media::MediaLogEvent::MEDIA_WARNING_LOG_ENTRY);
    event.params.SetString("warning", msg);
    return event;
  }

  DISALLOW_COPY_AND_ASSIGN(InspectorMediaEventHandlerTest);
};

bool operator==(const blink::InspectorPlayerProperty& lhs,
                const blink::InspectorPlayerProperty& rhs) {
  return lhs.name == rhs.name && lhs.value == rhs.value;
}

bool operator!=(const blink::InspectorPlayerProperty& lhs,
                const blink::InspectorPlayerProperty& rhs) {
  return !(lhs == rhs);
}

bool operator==(const blink::InspectorPlayerEvent& lhs,
                const blink::InspectorPlayerEvent& rhs) {
  return lhs.type == rhs.type && lhs.timestamp == rhs.timestamp &&
         lhs.key == rhs.key && lhs.value == rhs.value;
}

bool operator!=(const blink::InspectorPlayerEvent& lhs,
                const blink::InspectorPlayerEvent& rhs) {
  return !(lhs == rhs);
}

MATCHER_P(PropertiesEqualTo, props, "") {
  if (props.size() != arg.size())
    return false;
  for (size_t i = 0; i < props.size(); i++)
    if (props[i] != arg[i])
      return false;
  return true;
}

MATCHER_P(EventsEqualTo, events, "") {
  if (events.size() != arg.size())
    return false;
  for (size_t i = 0; i < events.size(); i++)
    if (events[i] != arg[i])
      return false;
  return true;
}

TEST_F(InspectorMediaEventHandlerTest, ConvertsProperties) {
  std::vector<media::MediaLogEvent> events = {
      CreatePropChangeEvent({{"test_key", "test_value"}})};

  blink::InspectorPlayerProperties expected;
  blink::InspectorPlayerProperty prop = {
      blink::WebString::FromUTF8("test_key"),
      blink::WebString::FromUTF8("test_value")};
  expected.emplace_back(prop);
  EXPECT_CALL(*mock_context_,
              MockSetPlayerProperties(PropertiesEqualTo(expected)))
      .Times(1);
  EXPECT_CALL(*mock_context_, MockNotifyPlayerEvents(_)).Times(0);

  handler_->SendQueuedMediaEvents(events);
}

TEST_F(InspectorMediaEventHandlerTest, SplitsDoubleProperties) {
  std::vector<media::MediaLogEvent> events = {
      CreatePropChangeEvent({{"test_key", "test_value"}, {"foo", "bar"}})};

  blink::InspectorPlayerProperties expected;
  blink::InspectorPlayerProperty prop_test = {
      blink::WebString::FromUTF8("test_key"),
      blink::WebString::FromUTF8("test_value")};
  blink::InspectorPlayerProperty prop_foo = {blink::WebString::FromUTF8("foo"),
                                             blink::WebString::FromUTF8("bar")};
  expected.emplace_back(prop_foo);
  expected.emplace_back(prop_test);
  EXPECT_CALL(*mock_context_,
              MockSetPlayerProperties(PropertiesEqualTo(expected)))
      .Times(1);
  EXPECT_CALL(*mock_context_, MockNotifyPlayerEvents(_)).Times(0);

  handler_->SendQueuedMediaEvents(events);
}

TEST_F(InspectorMediaEventHandlerTest, ConvertsMessageEvent) {
  std::vector<media::MediaLogEvent> events = {
      CreateLogEvent("Has Anyone Really Been Far Even as Decided to Use Even "
                     "Go Want to do Look More Like?")};

  blink::InspectorPlayerEvents expected;
  blink::InspectorPlayerEvent e = {
      blink::InspectorPlayerEvent::MESSAGE_EVENT, base::TimeTicks(),
      blink::WebString::FromUTF8("warning"),
      blink::WebString::FromUTF8("Has Anyone Really Been Far Even as Decided "
                                 "to Use Even Go Want to do Look More Like?")};
  expected.emplace_back(e);

  EXPECT_CALL(*mock_context_, MockSetPlayerProperties(_)).Times(0);
  EXPECT_CALL(*mock_context_, MockNotifyPlayerEvents(EventsEqualTo(expected)))
      .Times(1);

  handler_->SendQueuedMediaEvents(events);
}

TEST_F(InspectorMediaEventHandlerTest, ConvertsEventsAndProperties) {
  std::vector<media::MediaLogEvent> events = {
      CreateLogEvent("100% medically accurate"),
      CreatePropChangeEvent(
          {{"free_puppies", "all_taken"}, {"illuminati", "confirmed"}})};

  blink::InspectorPlayerEvents expected_events;
  blink::InspectorPlayerEvent e = {
      blink::InspectorPlayerEvent::MESSAGE_EVENT, base::TimeTicks(),
      blink::WebString::FromUTF8("warning"),
      blink::WebString::FromUTF8("100% medically accurate")};
  expected_events.emplace_back(e);

  blink::InspectorPlayerProperties expected_properties;
  blink::InspectorPlayerProperty puppies = {
      blink::WebString::FromUTF8("free_puppies"),
      blink::WebString::FromUTF8("all_taken")};
  blink::InspectorPlayerProperty illumanati = {
      blink::WebString::FromUTF8("illuminati"),
      blink::WebString::FromUTF8("confirmed")};
  expected_properties.emplace_back(puppies);
  expected_properties.emplace_back(illumanati);

  EXPECT_CALL(*mock_context_,
              MockSetPlayerProperties(PropertiesEqualTo(expected_properties)))
      .Times(1);
  EXPECT_CALL(*mock_context_,
              MockNotifyPlayerEvents(EventsEqualTo(expected_events)))
      .Times(1);

  handler_->SendQueuedMediaEvents(events);
}

TEST_F(InspectorMediaEventHandlerTest, PassesPlayAndPauseEvents) {
  std::vector<media::MediaLogEvent> events = {
      CreateEvent(media::MediaLogEvent::PLAY),
      CreateEvent(media::MediaLogEvent::PAUSE)};

  blink::InspectorPlayerEvents expected_events;
  blink::InspectorPlayerEvent play = {
      blink::InspectorPlayerEvent::PLAYBACK_EVENT, base::TimeTicks(),
      blink::WebString::FromUTF8("Event"), blink::WebString::FromUTF8("PLAY")};
  blink::InspectorPlayerEvent pause = {
      blink::InspectorPlayerEvent::PLAYBACK_EVENT, base::TimeTicks(),
      blink::WebString::FromUTF8("Event"), blink::WebString::FromUTF8("PAUSE")};
  expected_events.emplace_back(play);
  expected_events.emplace_back(pause);

  EXPECT_CALL(*mock_context_,
              MockNotifyPlayerEvents(EventsEqualTo(expected_events)))
      .Times(1);

  handler_->SendQueuedMediaEvents(events);
}

}  // namespace content
