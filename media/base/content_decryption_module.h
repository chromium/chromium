// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_CONTENT_DECRYPTION_MODULE_H_
#define MEDIA_BASE_CONTENT_DECRYPTION_MODULE_H_

#include <stdint.h>

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/functional/callback_forward.h"
#include "base/memory/ref_counted.h"
#include "media/base/cdm_key_information.h"
#include "media/base/eme_constants.h"
#include "media/base/media_export.h"
#include "url/gurl.h"

namespace base {
class Time;
}

namespace media {

class CdmContext;
struct ContentDecryptionModuleTraits;

template <typename... T>
class CdmPromiseTemplate;

typedef CdmPromiseTemplate<std::string> NewSessionCdmPromise;
typedef CdmPromiseTemplate<> SimpleCdmPromise;
typedef CdmPromiseTemplate<CdmKeyInformation::KeyStatus> KeyStatusCdmPromise;

typedef std::vector<std::unique_ptr<CdmKeyInformation>> CdmKeysInfo;

// Type of license required when creating/loading a session.
// Must be consistent with the values specified in the spec:
// https://w3c.github.io/encrypted-media/#idl-def-MediaKeySessionType
enum class CdmSessionType {
  kTemporary,
  kPersistentLicense,
  kMaxValue = kPersistentLicense
};

// Type of message being sent to the application.
// Must be consistent with the values specified in the spec:
// https://w3c.github.io/encrypted-media/#idl-def-MediaKeyMessageType
enum class CdmMessageType {
  LICENSE_REQUEST,
  LICENSE_RENEWAL,
  LICENSE_RELEASE,
  INDIVIDUALIZATION_REQUEST,
  MESSAGE_TYPE_MAX = INDIVIDUALIZATION_REQUEST
};

// This enum is reported to UKM. Existing values should NEVER be changed.
enum class HdcpVersion {
  kHdcpVersionNone = 0,
  kHdcpVersion1_0 = 1,
  kHdcpVersion1_1 = 2,
  kHdcpVersion1_2 = 3,
  kHdcpVersion1_3 = 4,
  kHdcpVersion1_4 = 5,
  kHdcpVersion2_0 = 6,
  kHdcpVersion2_1 = 7,
  kHdcpVersion2_2 = 8,
  kHdcpVersion2_3 = 9,
  kMaxValue = kHdcpVersion2_3
};

// Reasons for CDM session closed.
enum class CdmSessionClosedReason {
  kInternalError,  // An unrecoverable error happened in the CDM., e.g. crash.
  kClose,          // Reaction to MediaKeySession close().
  kReleaseAcknowledged,   // The CDM received a "record-of-license-destruction"
                          // acknowledgement.
  kHardwareContextReset,  // As a result of hardware context reset.
  kResourceEvicted,  // The CDM resource was evicted, e.g. by newer sessions.
  kMaxValue = kResourceEvicted
};

// An interface that represents the Content Decryption Module (CDM) in the
// Encrypted Media Extensions (EME) spec in Chromium.
// See http://w3c.github.io/encrypted-media/#cdm
//
// * Ownership
//
// This class is ref-counted. However, a ref-count should only be held by:
// - The owner of the CDM. This is usually some class in the EME stack, e.g.
//   CdmSessionAdapter in the render process, or MojoCdmService in a non-render
//   process.
// - The media player that uses the CDM, to prevent the CDM from being
//   destructed while still being used by the media player.
//
// When binding class methods into callbacks, prefer WeakPtr to using |this|
// directly to avoid having a ref-count held by the callback.
//
// * Thread Safety
//
// Most CDM operations happen on one thread. However, it is not uncommon that
// the media player lives on a different thread and may call into the CDM from
// that thread. For example, if the CDM supports a Decryptor interface, the
// Decryptor methods could be called on a different thread. The CDM
// implementation should make sure it's thread safe for these situations.
class MEDIA_EXPORT ContentDecryptionModule
    : public base::RefCountedThreadSafe<ContentDecryptionModule,
                                        ContentDecryptionModuleTraits> {
 public:
  ContentDecryptionModule(const ContentDecryptionModule&) = delete;
  ContentDecryptionModule& operator=(const ContentDecryptionModule&) = delete;

  // Provides a server certificate to be used to encrypt messages to the
  // license server.
  virtual void SetServerCertificate(
      const std::vector<uint8_t>& certificate,
      std::unique_ptr<SimpleCdmPromise> promise) = 0;

  // Gets the key status if there's a hypothetical key that requires the
  // |min_hdcp_version|. Resolve the |promise| with the key status after the
  // operation completes. Reject the |promise| if this operation is not
  // supported or unexpected error happened.
  virtual void GetStatusForPolicy(HdcpVersion min_hdcp_version,
                                  std::unique_ptr<KeyStatusCdmPromise> promise);

  // Creates a session with |session_type|. Then generates a request with the
  // |init_data_type| and |init_data|.
  // Note:
  // 1. The session ID will be provided when the |promise| is resolved.
  // 2. The generated request should be returned through a SessionMessageCB,
  //    which must be AFTER the |promise| is resolved. Otherwise, the session ID
  //    in the callback will not be recognized.
  // 3. UpdateSession(), CloseSession() and RemoveSession() should only be
  //    called after the |promise| is resolved.
  virtual void CreateSessionAndGenerateRequest(
      CdmSessionType session_type,
      EmeInitDataType init_data_type,
      const std::vector<uint8_t>& init_data,
      std::unique_ptr<NewSessionCdmPromise> promise) = 0;

  // Loads a session with the |session_id| provided. Resolves the |promise| with
  // |session_id| if the session is successfully loaded. Resolves the |promise|
  // with an empty session ID if the session cannot be found. Rejects the
  // |promise| if session loading is not supported, or other unexpected failure
  // happened.
  // Note: UpdateSession(), CloseSession() and RemoveSession() should only be
  //       called after the |promise| is resolved.
  virtual void LoadSession(CdmSessionType session_type,
                           const std::string& session_id,
                           std::unique_ptr<NewSessionCdmPromise> promise) = 0;

  // Updates a session specified by |session_id| with |response|.
  virtual void UpdateSession(const std::string& session_id,
                             const std::vector<uint8_t>& response,
                             std::unique_ptr<SimpleCdmPromise> promise) = 0;

  // Closes the session specified by |session_id|. The CDM should resolve or
  // reject the |promise| when the call has been processed. This may be before
  // the session is closed. Once the session is closed, a SessionClosedCB must
  // also be called.
  // Note that the EME spec executes the close() action asynchronously, so
  // CloseSession() may be called multiple times on the same session.
  virtual void CloseSession(const std::string& session_id,
                            std::unique_ptr<SimpleCdmPromise> promise) = 0;

  // Removes stored session data associated with the session specified by
  // |session_id|.
  virtual void RemoveSession(const std::string& session_id,
                             std::unique_ptr<SimpleCdmPromise> promise) = 0;

  // Returns the CdmContext associated with |this|. The returned CdmContext is
  // owned by |this| and the caller needs to make sure it is not used after
  // |this| is destructed. This method should never return null.
  virtual CdmContext* GetCdmContext() = 0;

  // Deletes |this| on the correct thread. By default |this| is deleted
  // immediately. Override this method if |this| needs to be deleted on a
  // specific thread.
  virtual void DeleteOnCorrectThread() const;

 protected:
  friend class base::RefCountedThreadSafe<ContentDecryptionModule,
                                          ContentDecryptionModuleTraits>;

  ContentDecryptionModule();
  virtual ~ContentDecryptionModule();
};

struct MEDIA_EXPORT ContentDecryptionModuleTraits {
  // Destroys |cdm| on the correct thread.
  static void Destruct(const ContentDecryptionModule* cdm);
};

// Try to convert `hdcp_version_string` to `HdcpVersion`. Returns std::nullopt
// on failure.
MEDIA_EXPORT std::optional<media::HdcpVersion> MaybeHdcpVersionFromString(
    const std::string& hdcp_version_string);

// CDM session event callbacks.

// Called when the CDM needs to queue a message event to the session object.
// See http://w3c.github.io/encrypted-media/#dom-evt-message
using SessionMessageCB =
    base::RepeatingCallback<void(const std::string& session_id,
                                 CdmMessageType message_type,
                                 const std::vector<uint8_t>& message)>;

// Called when the session specified by `session_id` is closed. Note that the
// CDM may close a session at any point, such as in response to a CloseSession()
// call, when the session is no longer needed, or when system resources are
// lost, as specified by `reason`.
// See http://w3c.github.io/encrypted-media/#session-closed
using SessionClosedCB =
    base::RepeatingCallback<void(const std::string& session_id,
                                 CdmSessionClosedReason reason)>;

// Called when there has been a change in the keys in the session or their
// status. See http://w3c.github.io/encrypted-media/#dom-evt-keystatuseschange
using SessionKeysChangeCB =
    base::RepeatingCallback<void(const std::string& session_id,
                                 bool has_additional_usable_key,
                                 CdmKeysInfo keys_info)>;

// Called when the CDM changes the expiration time of a session.
// See http://w3c.github.io/encrypted-media/#update-expiration
// A null base::Time() will be translated to NaN in Javascript, which means "no
// such time exists or if the license explicitly never expires, as determined
// by the CDM", according to the EME spec.
using SessionExpirationUpdateCB =
    base::RepeatingCallback<void(const std::string& session_id,
                                 base::Time new_expiry_time)>;

}  // namespace media

#endif  // MEDIA_BASE_CONTENT_DECRYPTION_MODULE_H_
