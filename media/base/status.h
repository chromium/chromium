// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_STATUS_H_
#define MEDIA_BASE_STATUS_H_

#include <memory>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

#include "base/location.h"
#include "base/logging.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/values.h"
#include "media/base/media_export.h"
#include "media/base/media_serializers_base.h"

// Mojo namespaces for serialization friend declarations.
namespace mojo {
template <typename T, typename U>
struct StructTraits;
}  // namespace mojo

namespace media {

// See media/base/status.md for details and instructions for
// using TypedStatus<T>.

// This is the type that enum classes used for specializing |TypedStatus| must
// extend from.
using StatusCodeType = uint16_t;

// This is the type that TypedStatusTraits::Group should be.
using StatusGroupType = std::string_view;

// Concept declaring what is required of all types to be used as a parameter
// to a TypedStatus template.
template <typename T>
concept TypedStatusImplTraits =
    // T::Codes is an enum which has all the nested status codes in it
    std::is_enum_v<typename T::Codes> &&
    // T::Group() must exist and return a StatusGroupType
    requires {
      { T::Group() } -> std::same_as<StatusGroupType>;
    } &&
    // If T::OkEnumValue() is defined and T::Codes::kOk is a member of the
    // codes enumeration, T::OkEnumValue() must return T::Codes::kOk
    ([]() -> bool {
      if constexpr (!requires { T::Codes::kOk; }) {
        return true;
      } else if constexpr (!requires { &T::OkEnumValue(); }) {
        return true;
      } else {
        return T::OkEnumValue() == T::Codes::kOk;
      }
    }());

// Concept declaring that T = TypedStatus<V> is constructable from an F iff
// V::OnCreateFrom(T*, F) is declared.
template <typename T, typename F>
concept TypedStatusConstructableFrom = requires(T* t, F f) {
  { T::Traits::OnCreateFrom(t, f) } -> std::same_as<void>;
} && TypedStatusImplTraits<typename T::Traits>;

namespace internal {

struct MEDIA_EXPORT StatusData {
  StatusData();
  StatusData(const StatusData&);
  StatusData(StatusGroupType group,
             StatusCodeType code,
             std::string_view message);
  ~StatusData();
  StatusData& operator=(const StatusData&);

  std::unique_ptr<StatusData> copy() const;
  void AddLocation(const base::Location&);

  void RenderToLogWriter(logging::LogSeverity s = logging::LOGGING_ERROR) const;

  // Enum group ID.
  std::string group;

  // Entry within enum, cast to base type.
  StatusCodeType code;

  // The current error message (Can be used for
  // https://developer.mozilla.org/en-US/docs/Web/API/Status)
  std::string message;

  // Stack frames
  base::Value::List frames;

  // Store a root cause. Helpful for debugging, as it can end up containing
  // the chain of causes.
  std::unique_ptr<StatusData> cause;

  // Data attached to the error
  base::Value data;
};

// Helper class to allow traits with no default enum.
template <TypedStatusImplTraits T>
struct StatusTraitsHelper {
  // If T defines OkEnumValue(), then return it. Otherwise, return an
  // T::Codes::kOk if that's defined, or std::nullopt if its not.
  static constexpr std::optional<typename T::Codes> OkEnumValue() {
    if constexpr (requires { &T::OkEnumValue; }) {
      return T::OkEnumValue();
    } else if constexpr (requires { T::Codes::kOk; }) {
      return T::Codes::kOk;
    } else {
      return std::nullopt;
    }
  }

  static std::string RenderGroupAndCode(T::Codes code) {
    if constexpr (requires { &T::ReadableCodeName; }) {
      return base::StrCat({T::Group(), "::", T::ReadableCodeName(code)});
    } else {
      return base::StrCat(
          {T::Group(),
           "::", base::NumberToString(static_cast<StatusCodeType>(code))});
    }
  }
};

// Implicitly converts to an ok value for any implementation of TypedStatus.
struct OkStatusImplicitConstructionHelper {};

// For gtest, so it can print this.  Otherwise, it tries to convert to an
// integer for printing.  That'd be okay, except our implicit cast matches the
// attempt to convert to long long, and tries to get `T::kOk` for `long long`.
MEDIA_EXPORT std::ostream& operator<<(
    std::ostream& stream,
    const OkStatusImplicitConstructionHelper&);

}  // namespace internal

// Constant names for serialized TypedStatus<T>.
struct MEDIA_EXPORT StatusConstants {
  static const char kCodeKey[];
  static const char kGroupKey[];
  static const char kMsgKey[];
  static const char kStackKey[];
  static const char kDataKey[];
  static const char kCauseKey[];
  static const char kFileKey[];
  static const char kLineKey[];
};

// See media/base/status.md for details and instructions for using TypedStatus.
template <TypedStatusImplTraits T>
class MEDIA_EXPORT TypedStatus {
 public:
  // Required for some of the helper concepts that are declared above.
  using Traits = T;

  // Convenience aliases to allow, e.g., MyStatusType::Codes::kGreatDisturbance.
  using Codes = typename T::Codes;

  // See media/base/status.md for the ways that an instantiation of TypedStatus
  // can be constructed, since there are a few.

  // Default constructor to please the Mojo Gods.
  TypedStatus() = default;

  // Copy constructor (also as a sacrifice to Lord Mojo)
  TypedStatus(const TypedStatus<T>& copy) { *this = copy; }

  // Special constructor use by OkStatus() to implicitly be cast to any required
  // status type.
  TypedStatus(const internal::OkStatusImplicitConstructionHelper&)
      : TypedStatus() {}

  // Used to implicitly create a TypedStatus from a TypedStatus::Codes value.
  TypedStatus(Codes code, const base::Location& location = FROM_HERE)
      : TypedStatus(code, "", location) {}

  // Used to allow returning {TypedStatus::Codes::kValue, CastFrom} implicitly
  // iff TypedStatus::T::OnCreateFrom is implemented.
  template <typename D>
    requires(TypedStatusConstructableFrom<TypedStatus<T>, D>)
  TypedStatus(Codes code,
              const D& data,
              const base::Location& location = FROM_HERE)
      : TypedStatus(code, "", location) {
    DCHECK(data_);
    T::OnCreateFrom(this, data);
  }

  // Used to allow returning {TypedStatus::Codes::kValue, "message", CastFrom}
  // implicitly iff TypedStatus::T::OnCreateFrom is implemented.
  template <typename D>
    requires(TypedStatusConstructableFrom<TypedStatus<T>, D>)
  TypedStatus(Codes code,
              std::string message,
              const D& data,
              const base::Location& location = FROM_HERE)
      : TypedStatus(code, std::move(message), location) {
    DCHECK(data_);
    T::OnCreateFrom(this, data);
  }

  // Used to allow returning {TypedStatus::Codes::kValue, cause}
  template <TypedStatusImplTraits O>
    requires(!std::is_same_v<O, T>)
  TypedStatus(Codes code,
              TypedStatus<O>&& cause,
              const base::Location& location = FROM_HERE)
      : TypedStatus(code, "", location) {
    DCHECK(data_);
    AddCause(std::move(cause));
  }

  // Used to allow returning {TypedStatus::Codes::kValue, "message", cause}
  template <TypedStatusImplTraits O>
  TypedStatus(Codes code,
              std::string message,
              TypedStatus<O>&& cause,
              const base::Location& location = FROM_HERE)
      : TypedStatus(code, std::move(message), location) {
    DCHECK(data_);
    AddCause(std::move(cause));
  }

  // Constructor to create a new TypedStatus from a numeric code & message.
  // These are immutable; if you'd like to change them, then you likely should
  // create a new TypedStatus.
  // NOTE: This should never be given a location parameter when called - It is
  // defaulted in order to grab the caller location.
  // Also used to allow returning {TypedStatus::Codes::kValue, "message"}
  // implicitly as a typed status.
  TypedStatus(Codes code,
              std::string message,
              const base::Location& location = FROM_HERE) {
    // Note that |message| would be dropped when code is the default value,
    // so DCHECK that it is not set.
    if (code == internal::StatusTraitsHelper<Traits>::OkEnumValue()) {
      DCHECK(!!message.empty());
      return;
    }
    data_ = std::make_unique<internal::StatusData>(
        internal::StatusTraitsHelper<Traits>::RenderGroupAndCode(code),
        static_cast<StatusCodeType>(code), std::move(message));
    data_->AddLocation(location);
  }

  TypedStatus<T>& operator=(const TypedStatus<T>& copy) {
    if (!copy.data_) {
      data_.reset();
      return *this;
    }
    data_ = copy.data_->copy();
    return *this;
  }

  bool is_ok() const { return !data_; }

  Codes code() const {
    if (!data_)
      return *internal::StatusTraitsHelper<Traits>::OkEnumValue();
    return static_cast<Codes>(data_->code);
  }

  std::string_view message() const {
    DCHECK(data_);
    return data_->message;
  }

  // Adds the current location to StatusBase as it’s passed upwards.
  // This does not need to be called at every location that touches it, but
  // should be called for those locations where the path is ambiguous or
  // critical. This can be especially helpful across IPC boundaries. This will
  // fail on an OK status.
  // NOTE: This should never be given a parameter when called - It is defaulted
  // in order to grab the caller location.
  TypedStatus<T>&& AddHere(const base::Location& location = FROM_HERE) && {
    DCHECK(data_);
    // We can't call MediaSerialize directly, because we can't include the
    // default serializers header, since it includes this header.
    data_->AddLocation(location);
    return std::move(*this);
  }

  // Allows us to append any datatype which can be converted to
  // an int/bool/string/base::Value. Any existing data associated with |key|
  // will be overwritten by |value|. This will fail on an OK status.
  template <typename D>
  TypedStatus<T>&& WithData(const char* key, const D& value) && {
    DCHECK(data_);
    data_->data.GetDict().Set(key, MediaSerialize(value));
    return std::move(*this);
  }

  template <typename D>
  void WithData(const char* key, const D& value) & {
    DCHECK(data_);
    data_->data.GetDict().Set(key, MediaSerialize(value));
  }

  // Add |cause| as the error that triggered this one.
  template <TypedStatusImplTraits AnyTraitsType>
  TypedStatus<T>&& AddCause(TypedStatus<AnyTraitsType>&& cause) && {
    AddCause(std::move(cause));
    return std::move(*this);
  }

  // Add |cause| as the error that triggered this one.
  template <TypedStatusImplTraits AnyTraitsType>
  void AddCause(TypedStatus<AnyTraitsType>&& cause) & {
    DCHECK(data_ && cause.data_);
    data_->cause = std::move(cause.data_);
  }

  // Destroy this status and log it
  void Log() && { LogInternal(); }

  void DebugLog(int verbosity) const {
    if (VLOG_IS_ON(verbosity)) {
      LogInternal();
    }
  }

  inline bool operator==(Codes code) const { return code == this->code(); }

  inline bool operator!=(Codes code) const { return code != this->code(); }

  inline bool operator==(const TypedStatus<T>& other) const {
    return other.code() == code();
  }

  inline bool operator!=(const TypedStatus<T>& other) const {
    return other.code() != code();
  }

  template <typename O>
  class Or {
   private:
    template <typename X>
    struct OrTypeUnwrapper {
      using Type = Or<X>;
    };
    template <typename X>
    struct OrTypeUnwrapper<Or<X>> {
      using Type = Or<X>;
    };

   public:
    using ErrorType = TypedStatus;

    ~Or() = default;

    // Create an Or type implicitly from a TypedStatus
    Or(TypedStatus<T>&& error) : error_(std::move(error)) {
      // `error_` must not be ok.
      DCHECK(!error_->is_ok());
    }

    Or(const TypedStatus<T>& error) : error_(error) {
      DCHECK(!error_->is_ok());
    }

    // Create an Or type implicitly from the alternate O.
    Or(O&& value) : value_(std::move(value)) {}
    Or(const O& value) : value_(value) {}

    // Create an Or type explicitly from a code
    Or(typename T::Codes code, const base::Location& location = FROM_HERE)
        : error_(TypedStatus<T>(code, "", location)) {
      DCHECK(!error_->is_ok());
    }

    // Create an Or type implicitly from any brace-initializer list that could
    // have been used to create the typed status
    template <typename First, typename... Rest>
    Or(typename T::Codes code,
       const First& first,
       const Rest&... rest,
       const base::Location& location = FROM_HERE)
        : error_(TypedStatus<T>(code, first, rest..., location)) {
      DCHECK(!error_->is_ok());
    }

    // Move- and copy- construction and assignment are okay.
    Or(const Or&) = default;
    Or(Or&&) = default;
    Or& operator=(Or&) = default;
    Or& operator=(Or&&) = default;

    bool has_value() const { return value_.has_value(); }

    inline bool operator==(typename T::Codes code) const {
      // We can't use Or<T>::code() directly, since it might not be allowed
      // due to not having an OK or default code.
      if (error_)
        return error_->code() == code;
      return internal::StatusTraitsHelper<Traits>::OkEnumValue() == code;
    }

    inline bool operator!=(typename T::Codes code) const {
      return !(*this == code);
    }

    // Return the error, if we have one.
    // Callers should ensure that this |!has_value()|.
    TypedStatus<T> error() && {
      CHECK(error_);
      auto error = std::move(*error_);
      error_.reset();
      return error;
    }

    // Return the value, if we have one.
    // Callers should ensure that this |has_value()|.
    O value() && {
      CHECK(value_);
      auto value = std::move(std::get<0>(*value_));
      value_.reset();
      return value;
    }

    const O& operator->() const
      requires requires(O o) { o.operator->(); }
    {
      return std::get<0>(*value_);
    }

    const O* operator->() const
      requires(!requires(O o) { o.operator->(); })
    {
      return &std::get<0>(*value_);
    }

    const O& operator*() const {
      CHECK(value_);
      return std::get<0>(*value_);
    }

    typename T::Codes code() const {
      DCHECK(error_ || value_);
      using helper = internal::StatusTraitsHelper<Traits>;
      static_assert(
          helper::OkEnumValue().has_value(),
          "Cannot call Or::code() without OkEnumValue or kOk defined");
      return error_ ? error_->code() : *helper::OkEnumValue();
    }

    template <typename Fn,
              typename R = decltype(std::declval<Fn>()(std::declval<O>()))>
    typename OrTypeUnwrapper<R>::Type MapValue(Fn&& lambda) && {
      CHECK(error_ || value_);
      if (!has_value()) {
        auto error = std::move(*error_);
        error_.reset();
        return error;
      }
      auto value = std::move(std::get<0>(*value_));
      value_.reset();
      return std::invoke(std::forward<Fn>(lambda), std::move(value));
    }

   private:
    std::optional<TypedStatus<T>> error_;

    // We wrap |O| in a container so that windows COM wrappers work.
    // They override operator& and similar, and won't compile in a
    // std::optional.
    std::optional<std::tuple<O>> value_;
  };

 private:
  std::unique_ptr<internal::StatusData> data_;

  template <typename StatusEnum, typename DataView>
  friend struct mojo::StructTraits;

  // Allow media log to access the internals to generate debug info for users.
  friend class MediaLog;

  // Allow dumping TypedStatus<T> to string for debugging in tests.
  friend struct internal::MediaSerializerDebug<TypedStatus<T>>;

  // Allow AddCause.
  template <TypedStatusImplTraits O>
  friend class TypedStatus;

  void LogInternal() const {
    if (data_) {
      data_->RenderToLogWriter();
    } else {
      auto ok_code = internal::StatusTraitsHelper<Traits>::OkEnumValue();
      // Only status's created from a traits with an OK enum value should ever
      // have a null `data_` object.
      CHECK(ok_code.has_value());
      LOG(ERROR) << internal::StatusTraitsHelper<Traits>::RenderGroupAndCode(
          *ok_code);
    }
  }
};

template <typename T>
inline bool operator==(typename T::Codes code, const TypedStatus<T>& status) {
  return status == code;
}

template <typename T>
inline bool operator!=(typename T::Codes code, const TypedStatus<T>& status) {
  return status != code;
}

// Convenience function to return |kOk|.
// OK won't have a message, trace, or data associated with them, and DCHECK
// if they are added.
MEDIA_EXPORT internal::OkStatusImplicitConstructionHelper OkStatus();

}  // namespace media

#endif  // MEDIA_BASE_STATUS_H_
