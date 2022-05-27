// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_STATUS_H_
#define MEDIA_BASE_STATUS_H_

#include <memory>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include "base/location.h"
#include "base/strings/string_piece.h"
#include "base/values.h"
#include "media/base/crc_16.h"
#include "media/base/media_export.h"
#include "media/base/media_serializers_base.h"

// Mojo namespaces for serialization friend declarations.
namespace mojo {
template <typename T, typename U>
struct StructTraits;
}  // namespace mojo

#define POST_STATUS_AND_RETURN_ON_FAILURE(eval_to_status, cb, ret) \
  do {                                                             \
    const auto EVALUATED = (eval_to_status);                       \
    if (!EVALUATED.is_ok()) {                                      \
      cb.Run(std::move(EVALUATED));                                \
      return ret;                                                  \
    }                                                              \
  } while (0)

namespace media {

// See media/base/status.md for details and instructions for
// using TypedStatus<T>.

// This is the type that enum classes used for specializing |TypedStatus| must
// extend from.
using StatusCodeType = uint16_t;

// This is the type that TypedStatusTraits::Group should be.
using StatusGroupType = base::StringPiece;

// This is the type that a status will get serialized into for UKM purposes.
using UKMPackedType = uint64_t;

namespace internal {

template <typename T>
struct SecondArgType {};

template <typename R, typename A1, typename A2>
struct SecondArgType<R(A1, A2)> {
  using Type = A2;
};

union UKMPackHelper {
  struct bits {
    uint16_t group;
    StatusCodeType code;
    uint32_t extra_data;
  } __attribute__((packed)) bits;
  UKMPackedType packed;

  static_assert(sizeof(bits) == sizeof(packed));
};

struct MEDIA_EXPORT StatusData {
  StatusData();
  StatusData(const StatusData&);
  StatusData(StatusGroupType group,
             StatusCodeType code,
             std::string message,
             UKMPackedType root_cause);
  ~StatusData();
  StatusData& operator=(const StatusData&);

  std::unique_ptr<StatusData> copy() const;
  void AddLocation(const base::Location&);

  // Enum group ID.
  std::string group;

  // Entry within enum, cast to base type.
  StatusCodeType code;

  // The current error message (Can be used for
  // https://developer.mozilla.org/en-US/docs/Web/API/Status)
  std::string message;

  // Stack frames
  std::vector<base::Value> frames;

  // Store a root cause. Helpful for debugging, as it can end up containing
  // the chain of causes.
  std::unique_ptr<StatusData> cause;

  // Data attached to the error
  base::Value data;

  // The root-cause status, as packed for UKM.
  UKMPackedType packed_root_cause = 0;
};

// Helper class to allow traits with no default enum.
template <typename T>
struct StatusTraitsHelper {
  // If T defines DefaultEnumValue(), then return it. Otherwise, return an
  // empty optional.
  static constexpr absl::optional<typename T::Codes> DefaultEnumValue() {
    return DefaultEnumValueImpl(0);
  }

  // If T defined PackExtraData(), then evaluate it. Otherwise, return a default
  // value. |PackExtraData| is an optional method that can operate on the
  // internal status data in order to pack it into a 32-bit entry for UKM.
  static constexpr uint32_t PackExtraData(const StatusData& info) {
    return DefaultPackExtraData(info, 0);
  }

 private:
  // Call with an (ignored) int, which will choose the first one if it isn't
  // removed by SFINAE, else will use the varargs one below.
  template <typename X = T>
  static constexpr typename std::enable_if_t<
      std::is_pointer_v<decltype(&X::DefaultEnumValue)>,
      absl::optional<typename T::Codes>>
  DefaultEnumValueImpl(int) {
    // Make sure the signature is correct, just for sanity.
    static_assert(
        std::is_same<decltype(T::DefaultEnumValue), typename T::Codes()>::value,
        "TypedStatus::Traits::DefaultEnumValue() must return Traits::Codes.");
    return T::DefaultEnumValue();
  }

  static constexpr absl::optional<typename T::Codes> DefaultEnumValueImpl(...) {
    return {};
  }

  template <typename X = T>
  static constexpr
      typename std::enable_if_t<std::is_pointer_v<decltype(&X::PackExtraData)>,
                                uint32_t>
      DefaultPackExtraData(const StatusData& info, int) {
    static_assert(
        std::is_same_v<decltype(T::PackExtraData(info)), uint32_t>,
        "Traits::PackExtraData(const StatusData&) must return uint32_t");
    return T::PackExtraData(info);
  }

  static constexpr int DefaultPackExtraData(const StatusData&, ...) {
    return 0;
  }
};

template <typename, typename>
struct OkStatusDetectorHelper {
  constexpr static bool has_ok = false;
};

// Matches <T,T> if T::kOk exists.
template <typename T>
struct OkStatusDetectorHelper<T, decltype(T::kOk)> {
  constexpr static bool has_ok = true;
};

// Does T have a T::kOk?
template <typename T>
constexpr bool DoesHaveOkCode = OkStatusDetectorHelper<T, T>::has_ok;

// Implicitly converts to `kOk` TypedStatus, for any traits.  Also converts to
// the enum code 'kOk', for any enum that has a 'kOk'.
struct OkStatusImplicitConstructionHelper {
  template <typename T>
  operator T() const {
    return T::kOk;
  }
};

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
template <typename T>
class MEDIA_EXPORT TypedStatus {
  static_assert(std::is_enum<typename T::Codes>::value,
                "TypedStatus Traits::Codes must be an enum type.");
  static_assert(std::is_same<decltype(T::Group), StatusGroupType()>::value,
                "TypedStatus Traits::Group() must return StatusGroupType.");

  // Check that, if there is both `kOk` and a default value, that the default
  // value is `kOk`.
  constexpr static bool verify_default_okayness() {
    // Fancy new (c++17) thing: remember that 'if constexpr' short-circuits at
    // compile-time, so the later clauses don't have to be compilable if the
    // the earlier ones match.  Specifically, it's okay to reference `kOk` even
    // if `T::Codes` doesn't have `kOk`, since we check for it first.
    if constexpr (!internal::DoesHaveOkCode<typename T::Codes>)
      return true;
    else if constexpr (!internal::StatusTraitsHelper<T>::DefaultEnumValue())
      return true;
    else
      return T::DefaultEnumValue() == T::Codes::kOk;
  }
  static_assert(verify_default_okayness(),
                "If kOk is defined, then either no default, or default==kOk");

 public:
  // Convenience aliases to allow, e.g., MyStatusType::Codes::kGreatDisturbance.
  using Traits = T;
  using Codes = typename T::Codes;

  // See media/base/status.md for the ways that an instantiation of TypedStatus
  // can be constructed, since there are a few.

  // Default constructor to please the Mojo Gods.
  TypedStatus() : data_(nullptr) {}

  // Copy constructor (also as a sacrifice to Lord Mojo)
  TypedStatus(const TypedStatus<T>& copy) { *this = copy; }

  // Special constructor use by OkStatus() to implicitly be cast to any required
  // status type.
  TypedStatus(const internal::OkStatusImplicitConstructionHelper&)
      : TypedStatus(Codes::kOk) {}

  // Used to implicitly create a TypedStatus from a TypedStatus::Codes value.
  TypedStatus(Codes code,
              const base::Location& location = base::Location::Current())
      : TypedStatus(code, "", location) {}

  TypedStatus(std::tuple<Codes, base::StringPiece> pack,
              const base::Location& location = base::Location::Current())
      : TypedStatus(std::get<0>(pack), std::get<1>(pack), location) {}

  // Used to allow returning {TypedStatus::Codes::kValue, CastFrom} implicitly
  // iff TypedStatus::Traits::OnCreateFrom is implemented.
  template <
      typename _T = Traits,
      typename = std::enable_if<std::is_pointer_v<decltype(&_T::OnCreateFrom)>>>
  TypedStatus(
      Codes code,
      const typename internal::SecondArgType<decltype(_T::OnCreateFrom)>::Type&
          data,
      const base::Location& location = base::Location::Current())
      : TypedStatus(code, "", location) {
    // TODO(tmathmeyer) I think we can make this dcheck a static assert.
    DCHECK(data_);
    Traits::OnCreateFrom(this, data);
  }

  // Used to allow returning {TypedStatus::Codes::kValue, "message", CastFrom}
  // implicitly iff TypedStatus::Traits::OnCreateFrom is implemented.
  template <
      typename _T = Traits,
      typename = std::enable_if<std::is_pointer_v<decltype(&_T::OnCreateFrom)>>>
  TypedStatus(
      Codes code,
      base::StringPiece message,
      const typename internal::SecondArgType<decltype(_T::OnCreateFrom)>::Type&
          data,
      const base::Location& location = base::Location::Current())
      : TypedStatus(code, message, location) {
    DCHECK(data_);
    Traits::OnCreateFrom(this, data);
  }

  // Used to allow returning {TypedStatus::Codes::kValue, cause}
  template <typename CausalStatusType>
  TypedStatus(Codes code,
              TypedStatus<CausalStatusType>&& cause,
              const base::Location& location = base::Location::Current())
      : TypedStatus(code, "", location) {
    static_assert(!std::is_same_v<CausalStatusType, Traits>);
    DCHECK(data_);
    AddCause(std::move(cause));
  }

  // Used to allow returning {TypedStatus::Codes::kValue, "message", cause}
  template <typename CausalStatusType>
  TypedStatus(Codes code,
              base::StringPiece message,
              TypedStatus<CausalStatusType>&& cause,
              const base::Location& location = base::Location::Current())
      : TypedStatus(code, message, location) {
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
              base::StringPiece message,
              const base::Location& location = base::Location::Current()) {
    // Note that |message| would be dropped when code is the default value,
    // so DCHECK that it is not set.
    if (code == internal::StatusTraitsHelper<Traits>::DefaultEnumValue()) {
      DCHECK(!!message.empty());
      return;
    }
    data_ = std::make_unique<internal::StatusData>(
        Traits::Group(), static_cast<StatusCodeType>(code),
        std::string(message), 0);
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

  // If `Codes` has a `kOk` value, then check return true if we're `kOk`.  If
  // there is no `kOk`, or if we're some other value, then return false.
  bool is_ok() const {
    if constexpr (internal::DoesHaveOkCode<Codes>) {
      return code() == Codes::kOk;
    } else {
      return false;
    }
  }

  Codes code() const {
    if (!data_)
      return *internal::StatusTraitsHelper<Traits>::DefaultEnumValue();
    return static_cast<Codes>(data_->code);
  }

  const std::string group() const {
    return data_ ? data_->group : std::string(Traits::Group());
  }

  const std::string& message() const {
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
  TypedStatus<T>&& AddHere(
      const base::Location& location = base::Location::Current()) && {
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
    data_->data.SetKey(key, MediaSerialize(value));
    return std::move(*this);
  }

  template <typename D>
  void WithData(const char* key, const D& value) & {
    DCHECK(data_);
    data_->data.SetKey(key, MediaSerialize(value));
  }

  // Add |cause| as the error that triggered this one.
  template <typename AnyTraitsType>
  TypedStatus<T>&& AddCause(TypedStatus<AnyTraitsType>&& cause) && {
    AddCause(std::move(cause));
    return std::move(*this);
  }

  // Add |cause| as the error that triggered this one.
  template <typename AnyTraitsType>
  void AddCause(TypedStatus<AnyTraitsType>&& cause) & {
    DCHECK(data_ && cause.data_);
    // The |cause| status is about to lose it's type forever. If it has no
    // causes, it might be sourced as the "root cause" status when sending to
    // UKM later, so it must be pre-emptively packed.
    if (!cause.data_->cause) {
      // If |cause| has no cause, then it shouldn't have |packed_root_cause|
      // either.
      DCHECK_EQ(cause.data_->packed_root_cause, 0lu);
      data_->packed_root_cause = cause.PackForUkm();
    } else {
      // If |cause| has a cause, it should have taken that causes's root-cause
      // when it was added as a cause. Since we're adding |cause| as our cause
      // now, we should steal |cause|'s root cause to be out root cause.
      DCHECK_NE(cause.data_->packed_root_cause, 0lu);
      data_->packed_root_cause = cause.data_->packed_root_cause;
    }
    data_->cause = std::move(cause.data_);
  }

  template <typename UKMBuilder>
  void ToUKM(UKMBuilder& builder) const {
    builder.SetStatus(PackForUkm());
    if (data_)
      builder.SetRootCause(data_->packed_root_cause);
  }

  inline bool operator==(Codes code) const { return code == this->code(); }

  inline bool operator!=(Codes code) const { return code != this->code(); }

  inline bool operator==(const TypedStatus<T>& other) const {
    return other.code() == code();
  }

  inline bool operator!=(const TypedStatus<T>& other) const {
    return other.code() != code();
  }

  template <typename OtherType>
  class Or {
   private:
    template <typename X>
    struct OrTypeUnwrapper {
      using type = Or<X>;
    };
    template <typename X>
    struct OrTypeUnwrapper<Or<X>> {
      using type = Or<X>;
    };

   public:
    using ErrorType = TypedStatus;

    ~Or() = default;

    // Create an Or type implicitly from a TypedStatus
    Or(TypedStatus<T>&& error) : error_(std::move(error)) {
      // `error_` must not be `kOk`, if there is such a value.
      DCHECK(!error_->is_ok());
    }

    Or(const TypedStatus<T>& error) : error_(error) {
      DCHECK(!error_->is_ok());
    }

    // Create an Or type implicitly from the alternate OtherType.
    Or(OtherType&& value) : value_(std::move(value)) {}
    Or(const OtherType& value) : value_(value) {}

    // Create an Or type explicitly from a code
    Or(typename T::Codes code,
       const base::Location& location = base::Location::Current())
        : error_(TypedStatus<T>(code, "", location)) {
      DCHECK(!error_->is_ok());
    }

    // Create an Or type implicitly from any brace-initializer list that could
    // have been used to create the typed status
    template <typename First, typename... Rest>
    Or(typename T::Codes code,
       const First& first,
       const Rest&... rest,
       const base::Location& location = base::Location::Current())
        : error_(TypedStatus<T>(code, first, rest..., location)) {
      DCHECK(!error_->is_ok());
    }

    // Move- and copy- construction and assignment are okay.
    Or(const Or&) = default;
    Or(Or&&) = default;
    Or& operator=(Or&) = default;
    Or& operator=(Or&&) = default;

    bool has_value() const { return value_.has_value(); }
    bool has_error() const { return error_.has_value(); }

    // If we have an error, verify that `code` matches.  If we have a value,
    // then this should match if an only if `code` is `kOk`.  If there is no
    // `kOk`, then it does not match even if we have a value.
    inline bool operator==(typename T::Codes code) const {
      if constexpr (internal::DoesHaveOkCode<typename T::Codes>) {
        return code == this->code();
      } else {
        return error_ ? code == error_->code() : false;
      }
    }

    inline bool operator!=(typename T::Codes code) const {
      return !(*this == code);
    }

    // Return the error, if we have one.
    // Callers should ensure that this |has_error()|.
    TypedStatus<T> error() && {
      CHECK(error_);
      auto error = std::move(*error_);
      error_.reset();
      return error;
    }

    // Return the value, if we have one.
    // Callers should ensure that this |has_value()|.
    OtherType value() && {
      CHECK(value_);
      auto value = std::move(std::get<0>(*value_));
      value_.reset();
      return value;
    }

    // Return constref of the value, if we have one.
    // Callers should ensure that this |has_value()|.
    const OtherType& operator->() const {
      CHECK(value_);
      return std::get<0>(*value_);
    }

    const OtherType& operator*() const {
      CHECK(value_);
      return std::get<0>(*value_);
    }

    typename T::Codes code() const {
      DCHECK(error_ || value_);
      // It is invalid to call |code()| on an |Or| with a value that
      // is specialized in a TypedStatus with no `kOk`.  Instead, you should
      // explicitly call has_error() / error().code().
      static_assert(internal::DoesHaveOkCode<typename T::Codes>,
                    "Cannot call Or::code() if there is no kOk code.");
      // TODO: should this DCHECK(error_) if we don't have kOk?  It's not as
      // strong as the static_assert, but maybe we want to allow this for types
      // that don't have `kOk`.
      return error_ ? error_->code() : T::Codes::kOk;
    }

    template <typename FnType,
              typename ReturnType =
                  decltype(std::declval<FnType>()(std::declval<OtherType>())),
              typename OrReturn = typename OrTypeUnwrapper<ReturnType>::type>
    OrReturn MapValue(FnType&& lambda) && {
      CHECK(error_ || value_);
      if (has_error()) {
        auto error = std::move(*error_);
        error_.reset();
        return error;
      }
      CHECK(value_);
      auto value = std::move(std::get<0>(*value_));
      value_.reset();
      return lambda(std::move(value));
    }

    template <typename FnType,
              typename ReturnType =
                  decltype(std::declval<FnType>()(std::declval<OtherType>())),
              typename ConvertTo = typename ReturnType::ErrorType>
    ReturnType MapValue(
        FnType&& lambda,
        typename ConvertTo::Codes on_error,
        base::StringPiece message = "",
        base::Location location = base::Location::Current()) && {
      CHECK(error_ || value_);
      if (has_error()) {
        auto error = std::move(*error_);
        error_.reset();
        return ConvertTo(on_error, message, location)
            .AddCause(std::move(error));
      }
      CHECK(value_);
      auto value = std::move(std::get<0>(*value_));
      value_.reset();
      return lambda(std::move(value));
    }

   private:
    absl::optional<TypedStatus<T>> error_;

    // We wrap |OtherType| in a container so that windows COM wrappers work.
    // They override operator& and similar, and won't compile in a
    // absl::optional.
    absl::optional<std::tuple<OtherType>> value_;
  };

 private:
  std::unique_ptr<internal::StatusData> data_;

  template <typename StatusEnum, typename DataView>
  friend struct mojo::StructTraits;

  // Allow media-serialization
  friend struct internal::MediaSerializer<TypedStatus<T>>;

  // Allow AddCause.
  template <typename StatusEnum>
  friend class TypedStatus;

  UKMPackedType PackForUkm() const {
    internal::UKMPackHelper result;
    result.bits.group = crc16(Traits::Group().data());
    result.bits.code = static_cast<StatusCodeType>(code());
    if (data_) {
      result.bits.extra_data =
          internal::StatusTraitsHelper<Traits>::PackExtraData(*data_);
    }

    return result.packed;
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
