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
                          const blink::InspectorPlayerEvents& events) override {
    MockNotifyPlayerEvents(events);
  }

  void SetPlayerProperties(
      blink::WebString id,
      const blink::InspectorPlayerProperties& props) override {
    MockSetPlayerProperties(props);
  }

  void NotifyPlayerErrors(blink::WebString id,
                          const blink::InspectorPlayerErrors& errors) override {
    MockNotifyPlayerErrors(errors);
  }

  void NotifyPlayerMessages(
      blink::WebString id,
      const blink::InspectorPlayerMessages& messages) override {
    MockNotifyPlayerMessages(messages);
  }

  MOCK_METHOD1(MockNotifyPlayerEvents, void(blink::InspectorPlayerEvents));
  MOCK_METHOD1(MockSetPlayerProperties, void(blink::InspectorPlayerProperties));
  MOCK_METHOD1(MockNotifyPlayerErrors, void(blink::InspectorPlayerErrors));
  MOCK_METHOD1(MockNotifyPlayerMessages, void(blink::InspectorPlayerMessages));
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

  template <media::MediaLogEvent T>
  media::MediaLogRecord CreateEvent() {
    media::MediaLogRecord event;
    event.id = 0;
    event.type = media::MediaLogRecord::Type::kMediaEventTriggered;
    event.time = base::TimeTicks();
    event.params.SetString("event",
                           media::MediaLogEventTypeSupport<T>::TypeName());
    return event;
  }

  media::MediaLogRecord CreatePropChange(
      std::vector<std::pair<std::string, std::string>> props) {
    media::MediaLogRecord event;
    event.id = 0;
    event.type = media::MediaLogRecord::Type::kMediaPropertyChange;
    event.time = base::TimeTicks();
    for (auto p : props) {
      event.params.SetString(std::get<0>(p), std::get<1>(p));
    }
    return event;
  }

  media::MediaLogRecord CreateMessage(std::string msg) {
    media::MediaLogRecord event;
    event.id = 0;
    event.type = media::MediaLogRecord::Type::kMessage;
    event.time = base::TimeTicks();
    event.params.SetString("warning", msg);
    return event;
  }

  media::MediaLogRecord CreateError(int errorcode) {
    media::MediaLogRecord error;
    error.id = 0;
    error.type = media::MediaLogRecord::Type::kMediaStatus;
    error.time = base::TimeTicks();
    error.params.SetIntPath(media::MediaLog::kStatusText, errorcode);
    return error;
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
  return lhs.timestamp == rhs.timestamp && lhs.value == rhs.value;
}

bool operator!=(const blink::InspectorPlayerEvent& lhs,
                const blink::InspectorPlayerEvent& rhs) {
  return !(lhs == rhs);
}

bool operator==(const blink::InspectorPlayerMessage& lhs,
                const blink::InspectorPlayerMessage& rhs) {
  return lhs.level == rhs.level && lhs.message == rhs.message;
}

bool operator!=(const blink::InspectorPlayerMessage& lhs,
                const blink::InspectorPlayerMessage& rhs) {
  return !(lhs == rhs);
}

bool operator==(const blink::InspectorPlayerError& lhs,
                const blink::InspectorPlayerError& rhs) {
  return lhs.errorCode == rhs.errorCode;
}

bool operator!=(const blink::InspectorPlayerError& lhs,
                const blink::InspectorPlayerError& rhs) {
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

MATCHER_P(MessagesEqualTo, messages, "") {
  if (messages.size() != arg.size())
    return false;
  for (size_t i = 0; i < messages.size(); i++)
    if (messages[i] != arg[i])
      return false;
  return true;
}

MATCHER_P(ErrorsEqualTo, errors, "") {
  if (errors.size() != arg.size())
    return false;
  for (size_t i = 0; i < errors.size(); i++)
    if (errors[i] != arg[i])
      return false;
  return true;
}

TEST_F(InspectorMediaEventHandlerTest, ConvertsProperties) {
  std::vector<media::MediaLogRecord> events = {
      CreatePropChange({{"test_key", "test_value"}})};

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
  std::vector<media::MediaLogRecord> events = {
      CreatePropChange({{"test_key", "test_value"}, {"foo", "bar"}})};

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
  std::vector<media::MediaLogRecord> events = {
      CreateMessage("Has Anyone Really Been Far Even as Decided to Use Even "
                    "Go Want to do Look More Like?")};

  blink::InspectorPlayerMessages expected;
  blink::InspectorPlayerMessage e = {
      blink::InspectorPlayerMessage::Level::kWarning,
      blink::WebString::FromUTF8("Has Anyone Really Been Far Even as Decided "
                                 "to Use Even Go Want to do Look More Like?")};
  expected.emplace_back(e);

  EXPECT_CALL(*mock_context_, MockSetPlayerProperties(_)).Times(0);
  EXPECT_CALL(*mock_context_,
              MockNotifyPlayerMessages(MessagesEqualTo(expected)))
      .Times(1);

  handler_->SendQueuedMediaEvents(events);
}

TEST_F(InspectorMediaEventHandlerTest, ConvertsEventsAndProperties) {
  std::vector<media::MediaLogRecord> events = {
      CreateMessage("100% medically accurate"),
      CreatePropChange(
          {{"free_puppies", "all_taken"}, {"illuminati", "confirmed"}})};

  blink::InspectorPlayerMessages expected_messages;
  blink::InspectorPlayerMessage e = {
      blink::InspectorPlayerMessage::Level::kWarning,
      blink::WebString::FromUTF8("100% medically accurate")};
  expected_messages.emplace_back(e);

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
              MockNotifyPlayerMessages(MessagesEqualTo(expected_messages)))
      .Times(1);

  handler_->SendQueuedMediaEvents(events);
}

TEST_F(InspectorMediaEventHandlerTest, PassesPlayAndPauseEvents) {
  std::vector<media::MediaLogRecord> events = {
      CreateEvent<media::MediaLogEvent::kPlay>(),
      CreateEvent<media::MediaLogEvent::kPause>()};

  blink::InspectorPlayerEvents expected_events;
  blink::InspectorPlayerEvent play = {
      base::TimeTicks(), blink::WebString::FromUTF8("{\"event\":\"kPlay\"}")};
  blink::InspectorPlayerEvent pause = {
      base::TimeTicks(), blink::WebString::FromUTF8("{\"event\":\"kPause\"}")};
  expected_events.emplace_back(play);
  expected_events.emplace_back(pause);

  EXPECT_CALL(*mock_context_,
              MockNotifyPlayerEvents(EventsEqualTo(expected_events)))
      .Times(1);

  handler_->SendQueuedMediaEvents(events);
}

TEST_F(InspectorMediaEventHandlerTest, PassesErrorEvents) {
  std::vector<media::MediaLogRecord> errors = {CreateError(5), CreateError(7)};

  blink::InspectorPlayerErrors expected_errors;
  blink::InspectorPlayerError first = {
      blink::InspectorPlayerError::Type::kPipelineError,
      blink::WebString::FromUTF8("5")};
  blink::InspectorPlayerError second = {
      blink::InspectorPlayerError::Type::kPipelineError,
      blink::WebString::FromUTF8("7")};
  expected_errors.emplace_back(first);
  expected_errors.emplace_back(second);

  EXPECT_CALL(*mock_context_,
              MockNotifyPlayerErrors(ErrorsEqualTo(expected_errors)))
      .Times(1);

  handler_->SendQueuedMediaEvents(errors);
}

}  // namespace content
