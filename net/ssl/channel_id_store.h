// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_SSL_CHANNEL_ID_STORE_H_
#define NET_SSL_CHANNEL_ID_STORE_H_

#include <list>
#include <memory>
#include <string>

#include "base/callback.h"
#include "base/threading/thread_checker.h"
#include "base/time/time.h"
#include "crypto/ec_private_key.h"
#include "net/base/net_export.h"

namespace net {

// An interface for storing and retrieving channel ID keypairs.
// See https://tools.ietf.org/html/draft-balfanz-tls-channelid-01

// Owned only by a single ChannelIDService object, which is responsible
// for deleting it.
class NET_EXPORT ChannelIDStore {
 public:
  // The ChannelID class contains a keypair, along with the corresponding
  // hostname (server identifier) and creation time.
  class NET_EXPORT ChannelID {
   public:
    ChannelID();
    ChannelID(const std::string& server_identifier,
              base::Time creation_time,
              std::unique_ptr<crypto::ECPrivateKey> key);
    ChannelID(const ChannelID& other);
    ChannelID& operator=(const ChannelID& other);
    ~ChannelID();

    // Server identifier.
    const std::string& server_identifier() const { return server_identifier_; }
    // The time the keypair was created.
    base::Time creation_time() const { return creation_time_; }
    // Returns the keypair for the channel ID. This pointer is only valid for
    // the lifetime of the ChannelID object - the ECPrivateKey object remains
    // owned by the ChannelID object; no ownership is transferred.
    crypto::ECPrivateKey* key() const { return key_.get(); }

   private:
    std::string server_identifier_;
    base::Time creation_time_;
    std::unique_ptr<crypto::ECPrivateKey> key_;
  };

  typedef std::list<ChannelID> ChannelIDList;

  typedef base::OnceCallback<
      void(int, const std::string&, std::unique_ptr<crypto::ECPrivateKey>)>
      GetChannelIDCallback;
  typedef base::OnceCallback<void(const ChannelIDList&)>
      GetChannelIDListCallback;

  virtual ~ChannelIDStore();

  // GetChannelID may return the result synchronously through the
  // output parameters, in which case it will return either OK if a keypair is
  // found in the store, or ERR_FILE_NOT_FOUND if none is found.  If the
  // result cannot be returned synchronously, GetChannelID will
  // return ERR_IO_PENDING and the callback will be called with the result
  // asynchronously.
  virtual int GetChannelID(const std::string& server_identifier,
                           std::unique_ptr<crypto::ECPrivateKey>* key_result,
                           GetChannelIDCallback callback) = 0;

  // Adds the keypair for a hostname to the store.
  virtual void SetChannelID(std::unique_ptr<ChannelID> channel_id) = 0;

  // Removes a keypair from the store.
  virtual void DeleteChannelID(const std::string& server_identifier,
                               base::OnceClosure completion_callback) = 0;

  // Deletes the channel ID keypairs that have a creation_date greater than
  // or equal to |delete_begin| and less than |delete_end| and whose server
  // identifier matches the |domain_predicate|. If base::Time value is_null,
  // that side of the comparison is unbounded.
  virtual void DeleteForDomainsCreatedBetween(
      const base::Callback<bool(const std::string&)>& domain_predicate,
      base::Time delete_begin,
      base::Time delete_end,
      base::OnceClosure completion_callback) = 0;

  // Removes all channel ID keypairs from the store.
  virtual void DeleteAll(base::OnceClosure completion_callback) = 0;

  // Returns all channel ID keypairs.
  virtual void GetAllChannelIDs(GetChannelIDListCallback callback) = 0;

  // Signals to the backing store that any pending writes should be flushed.
  virtual void Flush() = 0;

  // Returns the number of keypairs in the store.  May return 0 if the backing
  // store is not loaded yet.
  // Public only for unit testing.
  virtual int GetChannelIDCount() = 0;

  // When invoked, instructs the store to keep session related data on
  // destruction.
  virtual void SetForceKeepSessionState() = 0;

  // Returns true if this ChannelIDStore is ephemeral, and false if it is
  // persistent.
  virtual bool IsEphemeral() = 0;

 protected:
  THREAD_CHECKER(thread_checker_);
};

}  // namespace net

#endif  // NET_SSL_CHANNEL_ID_STORE_H_
