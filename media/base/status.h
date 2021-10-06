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
#include "media/base/media_export.h"
#include "media/base/media_serializers_base.h"
#include "media/base/status_codes.h"

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
using StatusGroupType = base::StringPiece;

namespace internal {

struct MEDIA_EXPORT StatusData {
  StatusData();
  StatusData(const StatusData&);
  StatusData(StatusGroupType group, StatusCodeType code, std::string message);
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

  // Causes
  std::vector<StatusData> causes;

  // Data attached to the error
  base::Value data;
};

}  // namespace internal

// See media/base/status.md for details and instructions for using TypedStatus.
template <typename T>
class MEDIA_EXPORT TypedStatus {
  static_assert(std::is_enum<typename T::Codes>::value,
                "TypedStatus must only be specialized with enum types.");

 public:
  using Traits = T;
  using Codes = typename T::Codes;

  // default constructor to please the Mojo Gods.
  TypedStatus() = default;

  // Constructor to create a new TypedStatus from a numeric code & message.
  // These are immutable; if you'd like to change them, then you likely should
  // create a new TypedStatus.
  // NOTE: This should never be given a location parameter when called - It is
  // defaulted in order to grab the caller location.
  TypedStatus(Codes code,
              base::StringPiece message = "",
              const base::Location& location = base::Location::Current()) {
    // Note that |message| would be dropped when code is the default value,
    // so DCHECK that it is not set.
    if (code == Traits::DefaultEnumValue()) {
      DCHECK(!!message.empty());
      return;
    }
    data_ = std::make_unique<internal::StatusData>(
        Traits::Group(), static_cast<StatusCodeType>(code),
        std::string(message));
    data_->AddLocation(location);
  }

  TypedStatus(const TypedStatus<T>& copy) { *this = copy; }

  TypedStatus<T>& operator=(const TypedStatus<T>& copy) {
    if (!copy.data_) {
      data_.reset();
      return *this;
    }
    data_ = copy.data_->copy();
    return *this;
  }

  // DEPRECATED: check code() == ok value.
  bool is_ok() const { return !data_; }

  Codes code() const {
    if (!data_)
      return *Traits::DefaultEnumValue();
    return static_cast<Codes>(data_->code);
  }

  const std::string group() const {
    return data_ ? data_->group : Traits::Group();
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
    DCHECK(data_ && cause.data_);
    data_->causes.push_back(*cause.data_);
    return std::move(*this);
  }

  // Add |cause| as the error that triggered this one.
  template <typename AnyTraitsType>
  void AddCause(TypedStatus<AnyTraitsType>&& cause) & {
    DCHECK(data_ && cause.data_);
    data_->causes.push_back(*cause.data_);
  }

  inline bool operator==(T code) const { return code == this->code(); }

  inline bool operator!=(T code) const { return code != this->code(); }

  inline bool operator==(const TypedStatus<T>& other) const {
    return other.code() == code();
  }

  inline bool operator!=(const TypedStatus<T>& other) const {
    return other.code() != code();
  }

  template <typename OtherType>
  class Or {
   public:
    ~Or() = default;

    // Implicit constructors allow returning |OtherType| or |TypedStatus|
    // directly.
    Or(TypedStatus<T>&& error) : error_(std::move(error)) {
      // |T| must either not have a default code, or not be default
      DCHECK(!Traits::DefaultEnumValue() ||
             *Traits::DefaultEnumValue() != code());
    }
    Or(const TypedStatus<T>& error) : error_(error) {
      DCHECK(!Traits::DefaultEnumValue() ||
             *Traits::DefaultEnumValue() != code());
    }

    Or(OtherType&& value) : value_(std::move(value)) {}
    Or(const OtherType& value) : value_(value) {}
    Or(typename T::Codes code,
       const base::Location& location = base::Location::Current())
        : error_(TypedStatus<T>(code, "", location)) {
      DCHECK(!Traits::DefaultEnumValue() ||
             *Traits::DefaultEnumValue() != code);
    }

    // Move- and copy- construction and assignment are okay.
    Or(const Or&) = default;
    Or(Or&&) = default;
    Or& operator=(Or&) = default;
    Or& operator=(Or&&) = default;

    bool has_value() const { return value_.has_value(); }
    bool has_error() const { return error_.has_value(); }

    inline bool operator==(typename T::Codes code) const {
      return code == this->code();
    }

    inline bool operator!=(typename T::Codes code) const {
      return code != this->code();
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

    typename T::Codes code() const {
      DCHECK(error_ || value_);
      // It is invalid to call |code()| on an |Or| with a value that
      // is specialized in a TypedStatus with no DefaultEnumValue.
      DCHECK(error_ || Traits::DefaultEnumValue());
      return error_ ? error_->code() : *Traits::DefaultEnumValue();
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

  void SetInternalData(std::unique_ptr<internal::StatusData> data) {
    data_ = std::move(data);
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

// Define TypedStatus<StatusCode> as Status in the media namespace for
// backwards compatibility. Also define StatusOr as Status::Or for the
// same reason.
struct GeneralStatusTraits {
  using Codes = StatusCode;
  static constexpr StatusGroupType Group() { return "GeneralStatusCode"; }
  static constexpr absl::optional<StatusCode> DefaultEnumValue() {
    return StatusCode::kOk;
  }
};
using Status = TypedStatus<GeneralStatusTraits>;
template <typename T>
using StatusOr = Status::Or<T>;

// Convenience function to return |kOk|.
// OK won't have a message, trace, or data associated with them, and DCHECK
// if they are added.
MEDIA_EXPORT Status OkStatus();

}  // namespace media

#endif  // MEDIA_BASE_STATUS_H_
