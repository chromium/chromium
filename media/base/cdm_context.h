// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_CDM_CONTEXT_H_
#define MEDIA_BASE_CDM_CONTEXT_H_

#include <optional>

#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/unguessable_token.h"
#include "build/build_config.h"
#include "media/base/media_export.h"
#include "media/media_buildflags.h"

#if BUILDFLAG(IS_WIN)
#include "media/base/win/media_foundation_cdm_proxy.h"
#endif

#if BUILDFLAG(IS_CHROMEOS)
namespace chromeos {
class ChromeOsCdmContext;
}
#endif

namespace media {

class CallbackRegistration;
class Decryptor;
class MediaCryptoContext;

#if BUILDFLAG(IS_FUCHSIA)
class FuchsiaCdmContext;
#endif

// An interface representing the context that a media player needs from a
// content decryption module (CDM) to decrypt (and decode) encrypted buffers.
// Typically this will be passed to the media player (e.g. using SetCdm()).
//
// Lifetime: The returned raw pointers are only guaranteed to be valid when the
// CdmContext is alive, which is usually guaranteed by holding a CdmContextRef
// (see below).
//
// Thread Model: Since this interface is used in many different contexts (e.g.
// different processes or platforms), the thread model is not defined as part
// of this interface. Subclasses must ensure thread safety.
class MEDIA_EXPORT CdmContext {
 public:
  // Events happening in a CDM that a media player should be aware of.
  enum class Event {
    // A key is newly usable, e.g. new key available, or previously expired key
    // has been renewed, etc.
    kHasAdditionalUsableKey,

    // A hardware reset happened. Some hardware context, e.g. hardware decoder
    // context may be lost.
    kHardwareContextReset,
  };

  // Callback to notify the occurrence of an Event.
  using EventCB = base::RepeatingCallback<void(Event)>;

  CdmContext(const CdmContext&) = delete;
  CdmContext& operator=(const CdmContext&) = delete;

  virtual ~CdmContext();

  // Registers a callback which will be called when an event happens in the CDM.
  // Returns null if the registration fails, otherwise the caller should hold
  // the returned CallbackRegistration (see "Lifetime" notes below). Can be
  // called multiple times to register multiple callbacks, all of which will be
  // called when an event happens.
  // Notes:
  // - Lifetime: The caller should keep the returned CallbackRegistration object
  // to keep the callback registered. The callback will be unregistered upon the
  // destruction of the returned CallbackRegistration object. The returned
  // CallbackRegistration object can be destructed on any thread.
  // - Thread Model: Can be called on any thread. The registered callback will
  // always be called on the thread where RegisterEventCB() is called.
  // - TODO(xhwang): Not using base::RepeatingCallbackList because it is not
  // thread- safe. Consider refactoring base::RepeatingCallbackList to avoid
  // code duplication.
  virtual std::unique_ptr<CallbackRegistration> RegisterEventCB(
      EventCB event_cb);

  // Gets the Decryptor object associated with the CDM. Returns nullptr if the
  // CDM does not support a Decryptor (i.e. platform-based CDMs where decryption
  // occurs implicitly along with decoding).
  virtual Decryptor* GetDecryptor();

  // Returns an ID that can be used to find a remote CDM, in which case this CDM
  // serves as a proxy to the remote one. Returns std::nullopt when remote CDM
  // is not supported (e.g. this CDM is a local CDM).
  virtual std::optional<base::UnguessableToken> GetCdmId() const;

  static std::string CdmIdToString(const base::UnguessableToken* cdm_id);

#if BUILDFLAG(IS_WIN)
  // Returns whether the CDM requires Media Foundation-based media Renderer.
  // This is separate from GetMediaFoundationCdmProxy() since it needs to be
  // a sync call called in the render process to setup the media pipeline.
  virtual bool RequiresMediaFoundationRenderer();

  // Returns a MediaFoundationCdmProxy to expose an IMFTrustedInput instance for
  // use in a Media Foundation rendering pipeline. Returns nullptr if the CDM is
  // in an invalid state or if MediaFoundationCdmProxy is not available.
  virtual scoped_refptr<MediaFoundationCdmProxy> GetMediaFoundationCdmProxy();
#endif

#if BUILDFLAG(IS_ANDROID)
  // Returns a MediaCryptoContext that can be used by MediaCodec based decoders.
  virtual MediaCryptoContext* GetMediaCryptoContext();
#endif

#if BUILDFLAG(IS_FUCHSIA)
  // Returns FuchsiaCdmContext interface when the context is backed by Fuchsia
  // CDM. Otherwise returns nullptr.
  virtual FuchsiaCdmContext* GetFuchsiaCdmContext();
#endif

#if BUILDFLAG(IS_CHROMEOS)
  // Returns a ChromeOsCdmContext interface when the context is backed by the
  // ChromeOS CdmFactoryDaemon. Otherwise return nullptr.
  virtual chromeos::ChromeOsCdmContext* GetChromeOsCdmContext();
#endif

 protected:
  CdmContext();
};

// A reference holder to make sure the CdmContext is always valid as long as
// |this| is alive. Typically |this| will hold a reference (directly or
// indirectly) to the host, e.g. a ContentDecryptionModule.
// This class must be held on the same thread where the host lives. The raw
// CdmContext pointer returned by GetCdmContext() may be used on other threads
// if it's supported by the CdmContext implementation.
class MEDIA_EXPORT CdmContextRef {
 public:
  virtual ~CdmContextRef() {}

  // Returns the CdmContext which is guaranteed to be alive as long as |this| is
  // alive. This function should never return nullptr.
  virtual CdmContext* GetCdmContext() = 0;
};

}  // namespace media

#endif  // MEDIA_BASE_CDM_CONTEXT_H_
