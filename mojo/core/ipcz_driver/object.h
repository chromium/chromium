// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_CORE_IPCZ_DRIVER_OBJECT_H_
#define MOJO_CORE_IPCZ_DRIVER_OBJECT_H_

#include <cstddef>
#include <cstdint>
#include <tuple>

#include "base/containers/span.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "build/build_config.h"
#include "mojo/core/ipcz_api.h"
#include "mojo/core/system_impl_export.h"
#include "mojo/public/cpp/platform/platform_handle.h"
#include "third_party/ipcz/include/ipcz/ipcz.h"

namespace mojo::core::ipcz_driver {

class Transport;

// Common base class for objects managed by Mojo's ipcz driver.
class MOJO_SYSTEM_IMPL_EXPORT ObjectBase
    : public base::RefCountedThreadSafe<ObjectBase> {
 public:
  REQUIRE_ADOPTION_FOR_REFCOUNTED_TYPE();

  enum Type : uint32_t {
    // An ipcz transport endpoint.
    kTransport,

    // A wrapped shared memory region.
    kSharedBuffer,

    // An active mapping for a shared memory region. These objects are not
    // serializable and cannot be transmitted over a Transport.
    kSharedBufferMapping,

    // A PlatformHandle which can be transmitted as-is by the platform's Channel
    // implementation, out-of-band from message data. This is the only type of
    // driver object which can be emitted by the driver's Serialize(), and it's
    // the only type accepted by its Transmit(). This type is unused on Windows,
    // where all platform handles are encoded as inline message data during
    // serialization.
    kTransmissiblePlatformHandle,

    // A PlatformHandle which may or may not be transmissible by the platform's
    // Channel implementation, but which can at least be transformed into
    // something transmissible during serialization.
    kWrappedPlatformHandle,

    // A DataPipe instance used to emulate Mojo data pipes over ipcz portals.
    kDataPipe,

    // A MojoTrap instance used to emulate a Mojo trap. These objects are not
    // serializable and cannot be transmitted over a Transport.
    kMojoTrap,

    // An Invitation instance used to emulate Mojo process invitations. These
    // objects are not serializable and cannot be transmitted over a Transport.
    kInvitation,
  };

  explicit ObjectBase(Type type);

  Type type() const { return type_; }

  IpczDriverHandle handle() const {
    return reinterpret_cast<IpczDriverHandle>(this);
  }

  static ObjectBase* FromHandle(IpczDriverHandle handle) {
    return reinterpret_cast<ObjectBase*>(handle);
  }

  static IpczDriverHandle ReleaseAsHandle(scoped_refptr<ObjectBase> object) {
    return reinterpret_cast<IpczDriverHandle>(object.release());
  }

  static scoped_refptr<ObjectBase> TakeFromHandle(IpczDriverHandle handle) {
    scoped_refptr<ObjectBase> object(FromHandle(handle));
    if (object) {
      // We're inheriting a ref previously owned by `handle`, so drop the extra
      // ref we just added.
      object->Release();
    }
    return object;
  }

  // Peeks at `box` and returns a pointer to its underlying object. Does not
  // invalidate `box`.
  static ObjectBase* FromBox(IpczHandle box) {
    return FromHandle(PeekBox(box));
  }

  // Boxes a reference to `object` and returns an IpczHandle for the box.
  static IpczHandle Box(scoped_refptr<ObjectBase> object);

  // Closes this object.
  virtual void Close();

  // Indicates whether this object can be serialized at all.
  virtual bool IsSerializable() const;

  // Computes the number of bytes and platform handles required to serialize
  // this object for transmission through `transmitter`. Returns false if the
  // object cannot be serialized or transmitted as such.
  virtual bool GetSerializedDimensions(Transport& transmitter,
                                       size_t& num_bytes,
                                       size_t& num_handles);

  // Attempts to serialize this object into `data` and `handles` which are
  // already sufficiently sized according to GetSerializedDimensions(). Returns
  // false if serialization fails.
  virtual bool Serialize(Transport& transmitter,
                         base::span<uint8_t> data,
                         base::span<PlatformHandle> handles);

 protected:
  virtual ~ObjectBase();

  // Peeks at `box` and returns its underlying driver handle.
  static IpczDriverHandle PeekBox(IpczHandle box);

  // Unboxes `box` and returns a reference to the object it contained.
  static scoped_refptr<ObjectBase> Unbox(IpczHandle box);

 private:
  friend class base::RefCountedThreadSafe<ObjectBase>;

  const Type type_;
};

// Type-specific base class which builds on ObjectBase but which infers its Type
// from a static object_type() method defined by T.
template <typename T>
class Object : public ObjectBase {
 public:
  Object() : ObjectBase(T::object_type()) {}

  // Constructs a new T instance with the forwarded Args, and immediately boxes
  // a reference to it. Returns a handle to the new box.
  template <typename... Args>
  static IpczHandle MakeBoxed(Args&&... args) {
    return Box(base::MakeRefCounted<T>(std::forward<Args>(args)...));
  }

  static T* FromHandle(IpczDriverHandle handle) {
    ObjectBase* object = ObjectBase::FromHandle(handle);
    if (!object || object->type() != T::object_type()) {
      return nullptr;
    }
    return static_cast<T*>(object);
  }

  static scoped_refptr<T> TakeFromHandle(IpczDriverHandle handle) {
    scoped_refptr<T> object(FromHandle(handle));
    if (object) {
      // We're inheriting a ref previously owned by `handle`, so drop the extra
      // ref we just added.
      object->Release();
    }
    return object;
  }

  // Peeks at `box` and returns a pointer to its underlying T, if the underlying
  // driver object is in fact a T. Does not invalidate `box`.
  static T* FromBox(IpczHandle box) { return FromHandle(PeekBox(box)); }

  // Unboxes `box` and returns a reference to its underlying T. If `box` is not
  // a box that contains a T, this returns null.
  static scoped_refptr<T> Unbox(IpczHandle box) {
    scoped_refptr<T> object = base::WrapRefCounted(T::FromBox(box));
    if (object) {
      std::ignore = ObjectBase::Unbox(box);
    }
    return object;
  }

 protected:
  ~Object() override = default;
};

}  // namespace mojo::core::ipcz_driver

#endif  // MOJO_CORE_IPCZ_DRIVER_OBJECT_H_
