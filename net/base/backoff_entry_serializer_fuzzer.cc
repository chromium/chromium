// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/backoff_entry_serializer.h"

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <optional>

#include "base/json/json_reader.h"
#include "base/logging.h"
#include "base/time/tick_clock.h"
#include "base/time/time.h"
#include "net/base/backoff_entry.h"
#include "net/base/backoff_entry_serializer_fuzzer_input.pb.h"
#include "testing/libfuzzer/proto/json_proto_converter.h"
#include "testing/libfuzzer/proto/lpm_interface.h"

namespace net {

namespace {
struct Environment {
  Environment() { logging::SetMinLogLevel(logging::LOGGING_ERROR); }
};

class ProtoTranslator {
 public:
  explicit ProtoTranslator(const fuzz_proto::FuzzerInput& input)
      : input_(input) {}

  BackoffEntry::Policy policy() const {
    return PolicyFromProto(input_->policy());
  }
  base::Time parse_time() const {
    return base::Time() + base::Microseconds(input_->parse_time());
  }
  base::TimeTicks parse_time_ticks() const {
    return base::TimeTicks() + base::Microseconds(input_->parse_time());
  }
  base::Time serialize_time() const {
    return base::Time() + base::Microseconds(input_->serialize_time());
  }
  base::TimeTicks now_ticks() const {
    return base::TimeTicks() + base::Microseconds(input_->now_ticks());
  }
  std::optional<base::Value> serialized_entry() const {
    json_proto::JsonProtoConverter converter;
    std::string json_array = converter.Convert(input_->serialized_entry());
    std::optional<base::Value> value = base::JSONReader::Read(json_array);
    return value;
  }

 private:
  const raw_ref<const fuzz_proto::FuzzerInput> input_;

  static BackoffEntry::Policy PolicyFromProto(
      const fuzz_proto::BackoffEntryPolicy& policy) {
    BackoffEntry::Policy new_policy;
    new_policy.num_errors_to_ignore = policy.num_errors_to_ignore();
    new_policy.initial_delay_ms = policy.initial_delay_ms();
    new_policy.multiply_factor = policy.multiply_factor();
    new_policy.jitter_factor = policy.jitter_factor();
    new_policy.maximum_backoff_ms = policy.maximum_backoff_ms();
    new_policy.entry_lifetime_ms = policy.entry_lifetime_ms();
    new_policy.always_use_initial_delay = policy.always_use_initial_delay();
    return new_policy;
  }
};

class MockClock : public base::TickClock {
 public:
  MockClock() = default;
  ~MockClock() override = default;

  void SetNow(base::TimeTicks now) { now_ = now; }
  base::TimeTicks NowTicks() const override { return now_; }

 private:
  base::TimeTicks now_;
};

// Tests the "deserialize-reserialize" property. Deserializes a BackoffEntry
// from JSON, reserializes it, then deserializes again. Holding time constant,
// we check that the parsed BackoffEntry values are equivalent.
void TestDeserialize(const ProtoTranslator& translator) {
  // Attempt to convert the json_proto.ArrayValue to a base::Value.
  std::optional<base::Value> value = translator.serialized_entry();
  if (!value)
    return;
  DCHECK(value->is_list());

  BackoffEntry::Policy policy = translator.policy();

  MockClock clock;
  clock.SetNow(translator.parse_time_ticks());

  // Attempt to deserialize a BackoffEntry.
  std::unique_ptr<BackoffEntry> entry =
      BackoffEntrySerializer::DeserializeFromList(
          value->GetList(), &policy, &clock, translator.parse_time());
  if (!entry)
    return;

  base::Value::List reserialized =
      BackoffEntrySerializer::SerializeToList(*entry, translator.parse_time());

  // Due to fuzzy interpretation in BackoffEntrySerializer::
  // DeserializeFromList, we cannot assert that |*reserialized == *value|.
  // Rather, we can deserialize |reserialized| and check that some weaker
  // properties are preserved.
  std::unique_ptr<BackoffEntry> entry_reparsed =
      BackoffEntrySerializer::DeserializeFromList(reserialized, &policy, &clock,
                                                  translator.parse_time());
  CHECK(entry_reparsed);
  CHECK_EQ(entry_reparsed->failure_count(), entry->failure_count());
  CHECK_LE(entry_reparsed->GetReleaseTime(), entry->GetReleaseTime());
}

// Tests the "serialize-deserialize" property. Serializes an arbitrary
// BackoffEntry to JSON, deserializes to another BackoffEntry, and checks
// equality of the two entries. Our notion of equality is *very weak* and needs
// improvement.
void TestSerialize(const ProtoTranslator& translator) {
  BackoffEntry::Policy policy = translator.policy();

  // Serialize the BackoffEntry.
  BackoffEntry native_entry(&policy);
  base::Value::List serialized = BackoffEntrySerializer::SerializeToList(
      native_entry, translator.serialize_time());

  MockClock clock;
  clock.SetNow(translator.now_ticks());

  // Deserialize it.
  std::unique_ptr<BackoffEntry> deserialized_entry =
      BackoffEntrySerializer::DeserializeFromList(serialized, &policy, &clock,
                                                  translator.parse_time());
  // Even though SerializeToList was successful, we're not guaranteed to have a
  // |deserialized_entry|. One reason deserialization may fail is if the parsed
  // |absolute_release_time_us| is below zero.
  if (!deserialized_entry)
    return;

  // TODO(dmcardle) Develop a stronger equality check for BackoffEntry.

  // Note that while |BackoffEntry::GetReleaseTime| looks like an accessor, it
  // returns a |value that is computed based on a random double, so it's not
  // suitable for CHECK_EQ here. See |BackoffEntry::CalculateReleaseTime|.

  CHECK_EQ(native_entry.failure_count(), deserialized_entry->failure_count());
}
}  // namespace

DEFINE_PROTO_FUZZER(const fuzz_proto::FuzzerInput& input) {
  static Environment env;

  // Print the entire |input| protobuf if asked.
  if (getenv("LPM_DUMP_NATIVE_INPUT")) {
    std::cout << "input: " << input.DebugString();
  }

  ProtoTranslator translator(input);
  // Skip this input if any of the time values are infinite.
  if (translator.now_ticks().is_inf() || translator.parse_time().is_inf() ||
      translator.serialize_time().is_inf()) {
    return;
  }
  TestDeserialize(translator);
  TestSerialize(translator);
}

}  // namespace net
