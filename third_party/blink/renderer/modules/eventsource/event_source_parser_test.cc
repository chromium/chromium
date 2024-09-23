// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/eventsource/event_source_parser.h"

#include <string.h>

#include <string_view>

#include "base/ranges/algorithm.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/modules/eventsource/event_source.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "third_party/blink/renderer/platform/wtf/text/character_names.h"

namespace blink {

namespace {

struct EventOrReconnectionTimeSetting {
  enum Type {
    kEvent,
    kReconnectionTimeSetting,
  };

  EventOrReconnectionTimeSetting(const AtomicString& event,
                                 const String& data,
                                 const AtomicString& id)
      : type(kEvent), event(event), data(data), id(id), reconnection_time(0) {}
  explicit EventOrReconnectionTimeSetting(uint64_t reconnection_time)
      : type(kReconnectionTimeSetting), reconnection_time(reconnection_time) {}

  const Type type;
  const AtomicString event;
  const String data;
  const AtomicString id;
  const uint64_t reconnection_time;
};

class Client : public GarbageCollected<Client>,
               public EventSourceParser::Client {
 public:
  ~Client() override = default;
  const Vector<EventOrReconnectionTimeSetting>& Events() const {
    return events_;
  }
  void OnMessageEvent(const AtomicString& event,
                      const String& data,
                      const AtomicString& id) override {
    events_.push_back(EventOrReconnectionTimeSetting(event, data, id));
  }
  void OnReconnectionTimeSet(uint64_t reconnection_time) override {
    events_.push_back(EventOrReconnectionTimeSetting(reconnection_time));
  }

 private:
  Vector<EventOrReconnectionTimeSetting> events_;
};

class StoppingClient : public GarbageCollected<StoppingClient>,
                       public EventSourceParser::Client {
 public:
  ~StoppingClient() override = default;
  const Vector<EventOrReconnectionTimeSetting>& Events() const {
    return events_;
  }
  void SetParser(EventSourceParser* parser) { parser_ = parser; }
  void OnMessageEvent(const AtomicString& event,
                      const String& data,
                      const AtomicString& id) override {
    parser_->Stop();
    events_.push_back(EventOrReconnectionTimeSetting(event, data, id));
  }
  void OnReconnectionTimeSet(uint64_t reconnection_time) override {
    events_.push_back(EventOrReconnectionTimeSetting(reconnection_time));
  }

  void Trace(Visitor* visitor) const override {
    visitor->Trace(parser_);
    EventSourceParser::Client::Trace(visitor);
  }

 private:
  Member<EventSourceParser> parser_;
  Vector<EventOrReconnectionTimeSetting> events_;
};

class EventSourceParserTest : public testing::Test {
 protected:
  using Type = EventOrReconnectionTimeSetting::Type;
  EventSourceParserTest()
      : client_(MakeGarbageCollected<Client>()),
        parser_(
            MakeGarbageCollected<EventSourceParser>(AtomicString(), client_)) {}
  ~EventSourceParserTest() override = default;

  void Enqueue(std::string_view chars) { parser_->AddBytes(chars); }
  void EnqueueOneByOne(std::string_view chars) {
    for (char c : chars) {
      parser_->AddBytes(base::span_from_ref(c));
    }
  }

  const Vector<EventOrReconnectionTimeSetting>& Events() {
    return client_->Events();
  }

  EventSourceParser* Parser() { return parser_; }

  test::TaskEnvironment task_environment_;
  Persistent<Client> client_;
  Persistent<EventSourceParser> parser_;
};

TEST_F(EventSourceParserTest, EmptyMessageEventShouldNotBeDispatched) {
  Enqueue("\n");

  EXPECT_EQ(0u, Events().size());
  EXPECT_EQ(String(), Parser()->LastEventId());
}

TEST_F(EventSourceParserTest, DispatchSimpleMessageEvent) {
  Enqueue("data:hello\n\n");

  ASSERT_EQ(1u, Events().size());
  ASSERT_EQ(Type::kEvent, Events()[0].type);
  EXPECT_EQ("message", Events()[0].event);
  EXPECT_EQ("hello", Events()[0].data);
  EXPECT_EQ(String(), Events()[0].id);
  EXPECT_EQ(AtomicString(), Parser()->LastEventId());
}

TEST_F(EventSourceParserTest, ConstructWithLastEventId) {
  parser_ =
      MakeGarbageCollected<EventSourceParser>(AtomicString("hoge"), client_);
  EXPECT_EQ("hoge", Parser()->LastEventId());

  Enqueue("data:hello\n\n");

  ASSERT_EQ(1u, Events().size());
  ASSERT_EQ(Type::kEvent, Events()[0].type);
  EXPECT_EQ("message", Events()[0].event);
  EXPECT_EQ("hello", Events()[0].data);
  EXPECT_EQ("hoge", Events()[0].id);
  EXPECT_EQ("hoge", Parser()->LastEventId());
}

TEST_F(EventSourceParserTest, DispatchMessageEventWithLastEventId) {
  Enqueue("id:99\ndata:hello\n");
  EXPECT_EQ(String(), Parser()->LastEventId());

  Enqueue("\n");

  ASSERT_EQ(1u, Events().size());
  ASSERT_EQ(Type::kEvent, Events()[0].type);
  EXPECT_EQ("message", Events()[0].event);
  EXPECT_EQ("hello", Events()[0].data);
  EXPECT_EQ("99", Events()[0].id);
  EXPECT_EQ("99", Parser()->LastEventId());
}

TEST_F(EventSourceParserTest, LastEventIdCanBeUpdatedEvenWhenDataIsEmpty) {
  Enqueue("id:99\n");
  EXPECT_EQ(String(), Parser()->LastEventId());

  Enqueue("\n");

  ASSERT_EQ(0u, Events().size());
  EXPECT_EQ("99", Parser()->LastEventId());
}

TEST_F(EventSourceParserTest, DispatchMessageEventWithCustomEventType) {
  Enqueue("event:foo\ndata:hello\n\n");

  ASSERT_EQ(1u, Events().size());
  ASSERT_EQ(Type::kEvent, Events()[0].type);
  EXPECT_EQ("foo", Events()[0].event);
  EXPECT_EQ("hello", Events()[0].data);
}

TEST_F(EventSourceParserTest, RetryTakesEffectEvenWhenNotDispatching) {
  Enqueue("retry:999\n");
  ASSERT_EQ(1u, Events().size());
  ASSERT_EQ(Type::kReconnectionTimeSetting, Events()[0].type);
  ASSERT_EQ(999u, Events()[0].reconnection_time);
}

TEST_F(EventSourceParserTest, EventTypeShouldBeReset) {
  Enqueue("event:foo\ndata:hello\n\ndata:bye\n\n");

  ASSERT_EQ(2u, Events().size());
  ASSERT_EQ(Type::kEvent, Events()[0].type);
  EXPECT_EQ("foo", Events()[0].event);
  EXPECT_EQ("hello", Events()[0].data);

  ASSERT_EQ(Type::kEvent, Events()[1].type);
  EXPECT_EQ("message", Events()[1].event);
  EXPECT_EQ("bye", Events()[1].data);
}

TEST_F(EventSourceParserTest, DataShouldBeReset) {
  Enqueue("data:hello\n\n\n");

  ASSERT_EQ(1u, Events().size());
  ASSERT_EQ(Type::kEvent, Events()[0].type);
  EXPECT_EQ("message", Events()[0].event);
  EXPECT_EQ("hello", Events()[0].data);
}

TEST_F(EventSourceParserTest, LastEventIdShouldNotBeReset) {
  Enqueue("id:99\ndata:hello\n\ndata:bye\n\n");

  EXPECT_EQ("99", Parser()->LastEventId());
  ASSERT_EQ(2u, Events().size());
  ASSERT_EQ(Type::kEvent, Events()[0].type);
  EXPECT_EQ("message", Events()[0].event);
  EXPECT_EQ("hello", Events()[0].data);
  EXPECT_EQ("99", Events()[0].id);

  ASSERT_EQ(Type::kEvent, Events()[1].type);
  EXPECT_EQ("message", Events()[1].event);
  EXPECT_EQ("bye", Events()[1].data);
  EXPECT_EQ("99", Events()[1].id);
}

TEST_F(EventSourceParserTest, VariousNewLinesShouldBeAllowed) {
  EnqueueOneByOne("data:hello\r\n\rdata:bye\r\r");

  ASSERT_EQ(2u, Events().size());
  ASSERT_EQ(Type::kEvent, Events()[0].type);
  EXPECT_EQ("message", Events()[0].event);
  EXPECT_EQ("hello", Events()[0].data);

  ASSERT_EQ(Type::kEvent, Events()[1].type);
  EXPECT_EQ("message", Events()[1].event);
  EXPECT_EQ("bye", Events()[1].data);
}

TEST_F(EventSourceParserTest, RetryWithEmptyValueShouldRestoreDefaultValue) {
  // TODO(yhirano): This is unspecified in the spec. We need to update
  // the implementation or the spec. See https://crbug.com/587980.
  Enqueue("retry\n");
  ASSERT_EQ(1u, Events().size());
  ASSERT_EQ(Type::kReconnectionTimeSetting, Events()[0].type);
  EXPECT_EQ(EventSource::kDefaultReconnectDelay, Events()[0].reconnection_time);
}

TEST_F(EventSourceParserTest, NonDigitRetryShouldBeIgnored) {
  Enqueue("retry:a0\n");
  Enqueue("retry:xi\n");
  Enqueue("retry:2a\n");
  Enqueue("retry:09a\n");
  Enqueue("retry:1\b\n");
  Enqueue("retry:  1234\n");
  Enqueue("retry:456 \n");

  EXPECT_EQ(0u, Events().size());
}

TEST_F(EventSourceParserTest, UnrecognizedFieldShouldBeIgnored) {
  Enqueue("data:hello\nhoge:fuga\npiyo\n\n");

  ASSERT_EQ(1u, Events().size());
  ASSERT_EQ(Type::kEvent, Events()[0].type);
  EXPECT_EQ("message", Events()[0].event);
  EXPECT_EQ("hello", Events()[0].data);
}

TEST_F(EventSourceParserTest, CommentShouldBeIgnored) {
  Enqueue("data:hello\n:event:a\n\n");

  ASSERT_EQ(1u, Events().size());
  ASSERT_EQ(Type::kEvent, Events()[0].type);
  EXPECT_EQ("message", Events()[0].event);
  EXPECT_EQ("hello", Events()[0].data);
}

TEST_F(EventSourceParserTest, BOMShouldBeIgnored) {
  // This line is recognized because "\xef\xbb\xbf" is a BOM.
  Enqueue(
      "\xef\xbb\xbf"
      "data:hello\n");
  // This line is ignored because "\xef\xbb\xbf" is part of the field name.
  Enqueue(
      "\xef\xbb\xbf"
      "data:bye\n");
  Enqueue("\n");

  ASSERT_EQ(1u, Events().size());
  ASSERT_EQ(Type::kEvent, Events()[0].type);
  EXPECT_EQ("message", Events()[0].event);
  EXPECT_EQ("hello", Events()[0].data);
}

TEST_F(EventSourceParserTest, BOMShouldBeIgnored_OneByOne) {
  // This line is recognized because "\xef\xbb\xbf" is a BOM.
  EnqueueOneByOne(
      "\xef\xbb\xbf"
      "data:hello\n");
  // This line is ignored because "\xef\xbb\xbf" is part of the field name.
  EnqueueOneByOne(
      "\xef\xbb\xbf"
      "data:bye\n");
  EnqueueOneByOne("\n");

  ASSERT_EQ(1u, Events().size());
  ASSERT_EQ(Type::kEvent, Events()[0].type);
  EXPECT_EQ("message", Events()[0].event);
  EXPECT_EQ("hello", Events()[0].data);
}

TEST_F(EventSourceParserTest, ColonlessLineShouldBeTreatedAsNameOnlyField) {
  Enqueue("data:hello\nevent:a\nevent\n\n");

  ASSERT_EQ(1u, Events().size());
  ASSERT_EQ(Type::kEvent, Events()[0].type);
  EXPECT_EQ("message", Events()[0].event);
  EXPECT_EQ("hello", Events()[0].data);
}

TEST_F(EventSourceParserTest, AtMostOneLeadingSpaceCanBeSkipped) {
  Enqueue("data:  hello  \nevent:  type \n\n");

  ASSERT_EQ(1u, Events().size());
  ASSERT_EQ(Type::kEvent, Events()[0].type);
  EXPECT_EQ(" type ", Events()[0].event);
  EXPECT_EQ(" hello  ", Events()[0].data);
}

TEST_F(EventSourceParserTest, DataShouldAccumulate) {
  Enqueue("data\ndata:hello\ndata: world\ndata\n\n");

  ASSERT_EQ(1u, Events().size());
  ASSERT_EQ(Type::kEvent, Events()[0].type);
  EXPECT_EQ("message", Events()[0].event);
  EXPECT_EQ("\nhello\nworld\n", Events()[0].data);
}

TEST_F(EventSourceParserTest, EventShouldNotAccumulate) {
  Enqueue("data:hello\nevent:a\nevent:b\n\n");

  ASSERT_EQ(1u, Events().size());
  ASSERT_EQ(Type::kEvent, Events()[0].type);
  EXPECT_EQ("b", Events()[0].event);
  EXPECT_EQ("hello", Events()[0].data);
}

TEST_F(EventSourceParserTest, FeedDataOneByOne) {
  EnqueueOneByOne(
      "data:hello\r\ndata:world\revent:a\revent:b\nid:4\n\nid:8\ndata:"
      "bye\r\n\r");

  ASSERT_EQ(2u, Events().size());
  ASSERT_EQ(Type::kEvent, Events()[0].type);
  EXPECT_EQ("b", Events()[0].event);
  EXPECT_EQ("hello\nworld", Events()[0].data);
  EXPECT_EQ("4", Events()[0].id);

  ASSERT_EQ(Type::kEvent, Events()[1].type);
  EXPECT_EQ("message", Events()[1].event);
  EXPECT_EQ("bye", Events()[1].data);
  EXPECT_EQ("8", Events()[1].id);
}

TEST_F(EventSourceParserTest, InvalidUTF8Sequence) {
  Enqueue("data:\xffhello\xc2\ndata:bye\n\n");

  ASSERT_EQ(1u, Events().size());
  ASSERT_EQ(Type::kEvent, Events()[0].type);
  EXPECT_EQ("message", Events()[0].event);
  String expected = String() + kReplacementCharacter + "hello" +
                    kReplacementCharacter + "\nbye";
  EXPECT_EQ(expected, Events()[0].data);
}

TEST(EventSourceParserStoppingTest, StopWhileParsing) {
  test::TaskEnvironment task_environment;
  StoppingClient* client = MakeGarbageCollected<StoppingClient>();
  EventSourceParser* parser =
      MakeGarbageCollected<EventSourceParser>(AtomicString(), client);
  client->SetParser(parser);

  const char kInput[] = "data:hello\nid:99\n\nid:44\ndata:bye\n\n";
  parser->AddBytes(base::span_from_cstring(kInput));

  const auto& events = client->Events();

  ASSERT_EQ(1u, events.size());
  ASSERT_EQ(EventOrReconnectionTimeSetting::Type::kEvent, events[0].type);
  EXPECT_EQ("message", events[0].event);
  EXPECT_EQ("hello", events[0].data);
  EXPECT_EQ("99", parser->LastEventId());
}

TEST_F(EventSourceParserTest, IgnoreIdHavingNullCharacter) {
  constexpr char input[] =
      "id:99\ndata:hello\n\nid:4\x0"
      "23\ndata:bye\n\n";
  // We can't use Enqueue because it relies on strlen.
  parser_->AddBytes(base::span_from_cstring(input));

  EXPECT_EQ("99", Parser()->LastEventId());
  ASSERT_EQ(2u, Events().size());
  ASSERT_EQ(Type::kEvent, Events()[0].type);
  EXPECT_EQ("message", Events()[0].event);
  EXPECT_EQ("hello", Events()[0].data);
  EXPECT_EQ("99", Events()[0].id);

  ASSERT_EQ(Type::kEvent, Events()[1].type);
  EXPECT_EQ("message", Events()[1].event);
  EXPECT_EQ("bye", Events()[1].data);
  EXPECT_EQ("99", Events()[1].id);
}

}  // namespace

}  // namespace blink
