// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_CDM_PROMISE_H_
#define MEDIA_BASE_CDM_PROMISE_H_

#include <stdint.h>

#include <string>

#include "base/check.h"
#include "base/logging.h"
#include "media/base/cdm_key_information.h"
#include "media/base/media_export.h"

namespace media {

// Interface for promises being resolved/rejected in response to various
// session actions. These may be called synchronously or asynchronously.
// The promise must be resolved or rejected exactly once. It is expected that
// the caller free the promise once it is resolved/rejected.

// These classes are almost generic, except for the parameters to reject(). If
// a generic class for promises is available, this could be changed to use the
// generic class as long as the parameters to reject() can be set appropriately.

// The base class only has a reject() method and GetResolveParameterType() that
// indicates the type of CdmPromiseTemplate. CdmPromiseTemplate<T> adds the
// resolve(T) method that is dependent on the type of promise. This base class
// is specified so that the promises can be easily saved before passing across
// IPC.
class MEDIA_EXPORT CdmPromise {
 public:
  enum class Exception {
    NOT_SUPPORTED_ERROR,
    INVALID_STATE_ERROR,
    QUOTA_EXCEEDED_ERROR,
    TYPE_ERROR,
    EXCEPTION_MAX = TYPE_ERROR
  };

  enum ResolveParameterType {
    VOID_TYPE,
    INT_TYPE,
    STRING_TYPE,
    KEY_STATUS_TYPE
  };

  // These values are reported to UMA. Never change existing values. Only add
  // new values at the bottom of the list. Note that values less than 1000000
  // are reserved for errors reported by the CDM and from 1100000 are specified
  // in MediaDrmBridge::MediaDrmSystemCode.
  // TODO(xhwang): Make SystemCode an enum class and pass |system_code| as
  // SystemCode everywhere.
  enum SystemCode : uint32_t {
    kMinValue = 1000000,  // To avoid conflict with system code reported by CDM.
    kOk = kMinValue,
    kFailure,
    kAborted,
    kConnectionError,
    kMaxValue = kConnectionError,
  };

  CdmPromise() = default;

  CdmPromise(const CdmPromise&) = delete;
  CdmPromise& operator=(const CdmPromise&) = delete;

  virtual ~CdmPromise() = default;

  // Used to indicate that the operation failed. |exception_code| must be
  // specified. |system_code| is a Key System-specific value for the error
  // that occurred, or 0 if there is no associated status code or such status
  // codes are not supported by the Key System. |error_message| is optional.
  virtual void reject(Exception exception_code,
                      uint32_t system_code,
                      const std::string& error_message) = 0;

  // Used to determine the template type of CdmPromiseTemplate<T> so that
  // saved CdmPromise objects can be cast to the correct templated version.
  virtual ResolveParameterType GetResolveParameterType() const = 0;
};

template <typename... T>
struct CdmPromiseTraits {};

template <>
struct MEDIA_EXPORT CdmPromiseTraits<> {
  static const CdmPromise::ResolveParameterType kType;
};

template <>
struct MEDIA_EXPORT CdmPromiseTraits<int> {
  static const CdmPromise::ResolveParameterType kType;
};

template <>
struct MEDIA_EXPORT CdmPromiseTraits<std::string> {
  static const CdmPromise::ResolveParameterType kType;
};

template <>
struct MEDIA_EXPORT CdmPromiseTraits<CdmKeyInformation::KeyStatus> {
  static const CdmPromise::ResolveParameterType kType;
};

// This class adds the resolve(T) method. This class is still an interface, and
// is used as the type of promise that gets passed around.
template <typename... T>
class CdmPromiseTemplate : public CdmPromise {
 public:
  CdmPromiseTemplate() : is_settled_(false) {}

  CdmPromiseTemplate(const CdmPromiseTemplate&) = delete;
  CdmPromiseTemplate& operator=(const CdmPromiseTemplate&) = delete;

  virtual ~CdmPromiseTemplate() { DCHECK(is_settled_); }

  virtual void resolve(const T&... result) = 0;

  // CdmPromise implementation.
  ResolveParameterType GetResolveParameterType() const final;

 protected:
  bool IsPromiseSettled() const { return is_settled_; }

  // All implementations must call this method in resolve() and reject() methods
  // to indicate that the promise has been settled.
  void MarkPromiseSettled() {
    // Promise can only be settled once.
    DCHECK(!is_settled_);
    is_settled_ = true;
  }

  // Must be called by the concrete destructor if !IsPromiseSettled().
  // Note: We can't call reject() in ~CdmPromise() because reject() is virtual.
  void RejectPromiseOnDestruction() {
    DCHECK(!is_settled_);
    std::string message =
        "Unfulfilled promise rejected automatically during destruction.";
    DVLOG(1) << message;
    reject(Exception::INVALID_STATE_ERROR, SystemCode::kAborted, message);
    DCHECK(is_settled_);
  }

 private:
  // Keep track of whether the promise has been resolved or rejected yet.
  bool is_settled_;
};

// Explicitly defining all variants of GetResolveParameterType().
// Without this component builds on Windows fail due to versions of the same
// method being generated in multiple DLLs.
template <>
MEDIA_EXPORT CdmPromise::ResolveParameterType
CdmPromiseTemplate<>::GetResolveParameterType() const;

template <>
MEDIA_EXPORT CdmPromise::ResolveParameterType
CdmPromiseTemplate<int>::GetResolveParameterType() const;

template <>
MEDIA_EXPORT CdmPromise::ResolveParameterType
CdmPromiseTemplate<std::string>::GetResolveParameterType() const;

template <>
MEDIA_EXPORT CdmPromise::ResolveParameterType CdmPromiseTemplate<
    CdmKeyInformation::KeyStatus>::GetResolveParameterType() const;

// A dummy CdmPromise that does nothing. Used for APIs requiring a CdmPromise
// while the result will be ignored.
template <typename... T>
class MEDIA_EXPORT DoNothingCdmPromise : public CdmPromiseTemplate<T...> {
 public:
  DoNothingCdmPromise() = default;
  DoNothingCdmPromise(const DoNothingCdmPromise&) = delete;
  DoNothingCdmPromise& operator=(const DoNothingCdmPromise&) = delete;
  ~DoNothingCdmPromise() override = default;

  // CdmPromiseTemplate.
  void resolve() final { CdmPromiseTemplate<T...>::MarkPromiseSettled(); }
  void reject(CdmPromise::Exception exception_code,
              uint32_t system_code,
              const std::string& error_message) final {
    CdmPromiseTemplate<T...>::MarkPromiseSettled();
  }
};

}  // namespace media

#endif  // MEDIA_BASE_CDM_PROMISE_H_
