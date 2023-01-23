// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/test/attribution_simulator_input_parser.h"

#include <stddef.h>
#include <stdint.h>

#include <ostream>
#include <string>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_piece.h"
#include "base/test/bind.h"
#include "base/time/time.h"
#include "base/types/expected.h"
#include "base/types/optional_util.h"
#include "base/values.h"
#include "components/attribution_reporting/source_registration_error.mojom.h"
#include "components/attribution_reporting/suitable_origin.h"
#include "components/attribution_reporting/trigger_registration.h"
#include "components/attribution_reporting/trigger_registration_error.mojom.h"
#include "content/browser/attribution_reporting/attribution_header_utils.h"
#include "content/browser/attribution_reporting/attribution_parser_test_utils.h"
#include "content/browser/attribution_reporting/attribution_source_type.h"
#include "content/browser/attribution_reporting/attribution_trigger.h"
#include "content/browser/attribution_reporting/storable_source.h"
#include "net/cookies/canonical_cookie.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/abseil-cpp/absl/types/variant.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {

namespace {

using ::attribution_reporting::SuitableOrigin;

constexpr char kTimestampKey[] = "timestamp";

class AttributionSimulatorInputParser {
 public:
  AttributionSimulatorInputParser(base::Time offset_time,
                                  std::ostream& error_stream)
      : offset_time_(offset_time), error_manager_(error_stream) {}

  ~AttributionSimulatorInputParser() = default;

  AttributionSimulatorInputParser(const AttributionSimulatorInputParser&) =
      delete;
  AttributionSimulatorInputParser(AttributionSimulatorInputParser&&) = delete;

  AttributionSimulatorInputParser& operator=(
      const AttributionSimulatorInputParser&) = delete;
  AttributionSimulatorInputParser& operator=(
      AttributionSimulatorInputParser&&) = delete;

  absl::optional<AttributionSimulationEvents> Parse(base::Value input) && {
    if (!EnsureDictionary(input))
      return absl::nullopt;

    static constexpr char kKeyCookies[] = "cookies";
    if (base::Value* cookies = input.GetDict().Find(kKeyCookies)) {
      auto context = PushContext(kKeyCookies);
      ParseList(
          std::move(*cookies),
          base::BindRepeating(&AttributionSimulatorInputParser::ParseCookie,
                              base::Unretained(this)));
    }

    static constexpr char kKeyDataClears[] = "data_clears";
    if (base::Value* data_clears = input.GetDict().Find(kKeyDataClears)) {
      auto context = PushContext(kKeyDataClears);
      ParseList(
          std::move(*data_clears),
          base::BindRepeating(&AttributionSimulatorInputParser::ParseDataClear,
                              base::Unretained(this)));
    }

    static constexpr char kKeySources[] = "sources";
    if (base::Value* sources = input.GetDict().Find(kKeySources)) {
      auto context = PushContext(kKeySources);
      ParseList(
          std::move(*sources),
          base::BindRepeating(&AttributionSimulatorInputParser::ParseSource,
                              base::Unretained(this)));
    }

    static constexpr char kKeyTriggers[] = "triggers";
    if (base::Value* triggers = input.GetDict().Find(kKeyTriggers)) {
      auto context = PushContext(kKeyTriggers);
      ParseList(
          std::move(*triggers),
          base::BindRepeating(&AttributionSimulatorInputParser::ParseTrigger,
                              base::Unretained(this)));
    }

    if (has_error())
      return absl::nullopt;

    return std::move(events_);
  }

 private:
  const base::Time offset_time_;
  AttributionParserErrorManager error_manager_;

  std::vector<AttributionSimulationEvent> events_;

  [[nodiscard]] std::unique_ptr<AttributionParserErrorManager::ScopedContext>
  PushContext(AttributionParserErrorManager::Context context) {
    return error_manager_.PushContext(context);
  }

  AttributionParserErrorManager::ErrorWriter Error() {
    return error_manager_.Error();
  }

  bool has_error() const { return error_manager_.has_error(); }

  template <typename T>
  void ParseList(T&& values,
                 base::RepeatingCallback<void(decltype(values))> callback) {
    if (!values.is_list()) {
      *Error() << "must be a list";
      return;
    }

    size_t index = 0;
    for (auto&& value : values.GetList()) {
      auto index_context = PushContext(index);
      callback.Run(std::forward<T>(value));
      index++;
    }
  }

  void ParseCookie(base::Value&& cookie) {
    if (!EnsureDictionary(cookie))
      return;

    const base::Value::Dict& dict = cookie.GetDict();

    base::Time time = ParseTime(dict, kTimestampKey);

    static constexpr char kKeyUrl[] = "url";
    GURL url = ParseURL(dict, kKeyUrl);
    if (!url.is_valid()) {
      auto context = PushContext(kKeyUrl);
      *Error() << "must be a valid URL";
    }

    static constexpr char kKeySetCookie[] = "Set-Cookie";
    const std::string* line = dict.FindString(kKeySetCookie);
    if (!line) {
      auto context = PushContext(kKeySetCookie);
      *Error() << "must be present";
      return;
    }

    // `CanonicalCookie::Create()` will DCHECK.
    if (time.is_null())
      return;

    std::unique_ptr<net::CanonicalCookie> canonical_cookie =
        net::CanonicalCookie::Create(url, *line, time,
                                     /*server_time=*/absl::nullopt,
                                     /*cookie_partition_key=*/absl::nullopt);
    if (!canonical_cookie)
      *Error() << "invalid cookie";

    if (has_error())
      return;

    events_.push_back(AttributionSimulatorCookie{
        .cookie = std::move(*canonical_cookie),
        .source_url = std::move(url),
    });
  }

  void ParseDataClear(base::Value&& data_clear) {
    if (!EnsureDictionary(data_clear))
      return;

    const base::Value::Dict& dict = data_clear.GetDict();

    base::Time time = ParseTime(dict, kTimestampKey);

    static constexpr char kKeyDeleteBegin[] = "delete_begin";
    base::Time delete_begin = base::Time::Min();
    if (dict.contains(kKeyDeleteBegin))
      delete_begin = ParseTime(dict, kKeyDeleteBegin);

    static constexpr char kKeyDeleteEnd[] = "delete_end";
    base::Time delete_end = base::Time::Max();
    if (dict.contains(kKeyDeleteEnd))
      delete_end = ParseTime(dict, kKeyDeleteEnd);

    absl::optional<base::flat_set<url::Origin>> origin_set;

    static constexpr char kKeyOrigins[] = "origins";
    if (const base::Value* origins = dict.Find(kKeyOrigins)) {
      auto context = PushContext(kKeyOrigins);
      origin_set.emplace();

      ParseList(
          *origins, base::BindLambdaForTesting([&](const base::Value& value) {
            if (!value.is_string()) {
              *Error() << "must be a string";
            } else {
              origin_set->emplace(url::Origin::Create(GURL(value.GetString())));
            }
          }));
    }

    bool delete_rate_limit_data =
        ParseBool(dict, "delete_rate_limit_data").value_or(true);

    if (has_error())
      return;

    events_.push_back(AttributionDataClear(time, delete_begin, delete_end,
                                           std::move(origin_set),
                                           delete_rate_limit_data));
  }

  void ParseSource(base::Value&& source) {
    if (!EnsureDictionary(source))
      return;

    base::Value::Dict& source_dict = source.GetDict();

    base::Time source_time = ParseTime(source_dict, kTimestampKey);
    absl::optional<SuitableOrigin> source_origin =
        ParseOrigin(source_dict, "source_origin");
    absl::optional<SuitableOrigin> reporting_origin =
        ParseOrigin(source_dict, "reporting_origin");
    absl::optional<AttributionSourceType> source_type =
        ParseSourceType(source_dict);

    if (has_error())
      return;

    ParseAttributionEvent(
        source_dict, "Attribution-Reporting-Register-Source",
        base::BindLambdaForTesting([&](base::Value::Dict dict) {
          base::expected<StorableSource,
                         attribution_reporting::mojom::SourceRegistrationError>
              storable_source = ParseSourceRegistration(
                  std::move(dict), source_time, std::move(*reporting_origin),
                  std::move(*source_origin), *source_type,
                  /*is_within_fenced_frame=*/false);

          if (!storable_source.has_value()) {
            *Error() << storable_source.error();
            return;
          }

          events_.push_back(std::move(*storable_source));
        }));
  }

  void ParseTrigger(base::Value&& trigger) {
    if (!EnsureDictionary(trigger))
      return;

    base::Value::Dict& trigger_dict = trigger.GetDict();

    base::Time trigger_time = ParseTime(trigger_dict, kTimestampKey);
    absl::optional<SuitableOrigin> reporting_origin =
        ParseOrigin(trigger_dict, "reporting_origin");
    absl::optional<SuitableOrigin> destination_origin =
        ParseOrigin(trigger_dict, "destination_origin");

    if (has_error())
      return;

    ParseAttributionEvent(
        trigger_dict, "Attribution-Reporting-Register-Trigger",
        base::BindLambdaForTesting([&](base::Value::Dict dict) {
          auto trigger_registration =
              attribution_reporting::TriggerRegistration::Parse(
                  std::move(dict));
          if (!trigger_registration.has_value()) {
            *Error() << trigger_registration.error();
            return;
          }

          events_.push_back(AttributionTriggerAndTime{
              .trigger = AttributionTrigger(std::move(*reporting_origin),
                                            std::move(*trigger_registration),
                                            std::move(*destination_origin),
                                            /*attestation=*/absl::nullopt,
                                            /*is_within_fenced_frame=*/false),
              .time = trigger_time,
          });
        }));
  }

  GURL ParseURL(const base::Value::Dict& dict, base::StringPiece key) const {
    if (const std::string* v = dict.FindString(key))
      return GURL(*v);

    return GURL();
  }

  absl::optional<SuitableOrigin> ParseOrigin(const base::Value::Dict& dict,
                                             base::StringPiece key) {
    auto context = PushContext(key);

    absl::optional<SuitableOrigin> origin;
    if (const std::string* s = dict.FindString(key))
      origin = SuitableOrigin::Deserialize(*s);

    if (!origin.has_value())
      *Error() << "must be a valid, secure origin";

    return origin;
  }

  base::Time ParseTime(const base::Value::Dict& dict, base::StringPiece key) {
    auto context = PushContext(key);

    const std::string* v = dict.FindString(key);
    int64_t milliseconds;

    if (v && base::StringToInt64(*v, &milliseconds)) {
      base::Time time = offset_time_ + base::Milliseconds(milliseconds);
      if (!time.is_null() && !time.is_inf())
        return time;
    }

    *Error() << "must be an integer number of milliseconds since the Unix "
                "epoch formatted as a base-10 string";
    return base::Time();
  }

  absl::optional<bool> ParseBool(const base::Value::Dict& dict,
                                 base::StringPiece key) {
    auto context = PushContext(key);

    const base::Value* v = dict.Find(key);
    if (!v) {
      return absl::nullopt;
    }

    if (!v->is_bool()) {
      *Error() << "must be a bool";
      return absl::nullopt;
    }

    return v->GetBool();
  }

  absl::optional<AttributionSourceType> ParseSourceType(
      const base::Value::Dict& dict) {
    static constexpr char kKey[] = "source_type";
    static constexpr char kNavigation[] = "navigation";
    static constexpr char kEvent[] = "event";

    auto context = PushContext(kKey);

    absl::optional<AttributionSourceType> source_type;

    if (const std::string* v = dict.FindString(kKey)) {
      if (*v == kNavigation) {
        source_type = AttributionSourceType::kNavigation;
      } else if (*v == kEvent) {
        source_type = AttributionSourceType::kEvent;
      }
    }

    if (!source_type) {
      *Error() << "must be either \"" << kNavigation << "\" or \"" << kEvent
               << "\"";
    }

    return source_type;
  }

  bool ParseAttributionEvent(
      base::Value::Dict& value,
      base::StringPiece key,
      base::OnceCallback<void(base::Value::Dict)> callback) {
    auto context = PushContext(key);

    base::Value* dict = value.Find(key);
    if (!dict) {
      *Error() << "must be present";
      return false;
    }

    if (!EnsureDictionary(*dict))
      return false;

    std::move(callback).Run(std::move(*dict).TakeDict());
    return true;
  }

  bool EnsureDictionary(const base::Value& value) {
    if (!value.is_dict()) {
      *Error() << "must be a dictionary";
      return false;
    }
    return true;
  }
};

}  // namespace

AttributionDataClear::AttributionDataClear(
    base::Time time,
    base::Time delete_begin,
    base::Time delete_end,
    absl::optional<base::flat_set<url::Origin>> origins,
    bool delete_rate_limit_data)
    : time(time),
      delete_begin(delete_begin),
      delete_end(delete_end),
      origins(std::move(origins)),
      delete_rate_limit_data(delete_rate_limit_data) {}

AttributionDataClear::~AttributionDataClear() = default;

AttributionDataClear::AttributionDataClear(const AttributionDataClear&) =
    default;

AttributionDataClear::AttributionDataClear(AttributionDataClear&&) = default;

AttributionDataClear& AttributionDataClear::operator=(
    const AttributionDataClear&) = default;

AttributionDataClear& AttributionDataClear::operator=(AttributionDataClear&&) =
    default;

absl::optional<AttributionSimulationEvents> ParseAttributionSimulationInput(
    base::Value input,
    const base::Time offset_time,
    std::ostream& error_stream) {
  return AttributionSimulatorInputParser(offset_time, error_stream)
      .Parse(std::move(input));
}

}  // namespace content
