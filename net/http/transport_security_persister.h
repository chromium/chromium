// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// TransportSecurityState maintains an in memory database containing the
// list of hosts that currently have transport security enabled. This
// singleton object deals with writing that data out to disk as needed and
// loading it at startup.

// At startup we need to load the transport security state from the
// disk. For the moment, we don't want to delay startup for this load, so we
// let the TransportSecurityState run for a while without being loaded.
// This means that it's possible for pages opened very quickly not to get the
// correct transport security information.
//
// To load the state, we schedule a Task on background_runner, which
// deserializes and configures the TransportSecurityState.
//
// The TransportSecurityState object supports running a callback function
// when it changes. This object registers the callback, pointing at itself.
//
// TransportSecurityState calls...
// TransportSecurityPersister::StateIsDirty
//   since the callback isn't allowed to block or reenter, we schedule a Task
//   on the file task runner after some small amount of time
//
// ...
//
// TransportSecurityPersister::SerializeState
//   copies the current state of the TransportSecurityState, serializes
//   and writes to disk.

#ifndef NET_HTTP_TRANSPORT_SECURITY_PERSISTER_H_
#define NET_HTTP_TRANSPORT_SECURITY_PERSISTER_H_

#include <optional>
#include <string>

#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/files/important_file_writer.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "net/base/net_export.h"
#include "net/http/transport_security_state.h"

namespace base {
class SequencedTaskRunner;
}

namespace net {

// Exists only to hold a "commit-interval" param. If disabled, the default
// ImportantFileWriter commit interval is used.
NET_EXPORT BASE_DECLARE_FEATURE(kTransportSecurityFileWriterSchedule);

// Reads and updates on-disk TransportSecurity state. Clients of this class
// should create, destroy, and call into it from one thread.
//
// background_runner is the task runner this class should use internally to
// perform file IO, and can optionally be associated with a different thread.
class NET_EXPORT TransportSecurityPersister
    : public TransportSecurityState::Delegate,
      public base::ImportantFileWriter::DataSerializer {
 public:
  // Create a TransportSecurityPersister with state |state| on background runner
  // |background_runner|. |data_path| points to the file to hold the transport
  // security state data on disk.
  TransportSecurityPersister(
      TransportSecurityState* state,
      const scoped_refptr<base::SequencedTaskRunner>& background_runner,
      const base::FilePath& data_path);

  TransportSecurityPersister(const TransportSecurityPersister&) = delete;
  TransportSecurityPersister& operator=(const TransportSecurityPersister&) =
      delete;

  ~TransportSecurityPersister() override;

  // Called by the TransportSecurityState when it changes its state.
  void StateIsDirty(TransportSecurityState*) override;
  // Called when the TransportSecurityState should be written immediately.
  // |callback| is called after data is persisted.
  void WriteNow(TransportSecurityState* state,
                base::OnceClosure callback) override;

  // ImportantFileWriter::DataSerializer:
  //
  // Serializes |transport_security_state_| into |*output|. Returns true if
  // all STS states were serialized correctly.
  //
  // The serialization format is JSON; the JSON represents a dictionary of
  // host:DomainState pairs (host is a string). The DomainState contains the STS
  // states and is represented as a dictionary containing the following keys and
  // value types (not all keys will always be present):
  //
  //     "sts_include_subdomains": true|false
  //     "created": double
  //     "expiry": double
  //     "mode": "default"|"force-https"
  //             legacy value synonyms "strict" = "force-https"
  //                                   "pinning-only" = "default"
  //             legacy value "spdy-only" is unused and ignored
  //     "report-uri": string
  //     "sts_observed": double
  //
  // Legacy data (see https://crbug.com/1232560) may also contain a top-level
  // "expect_ct" key, which will be deleted when read:
  //     "expect_ct": dictionary with keys:
  //         "expect_ct_expiry": double
  //         "expect_ct_observed": double
  //         "expect_ct_enforce": true|false
  //         "expect_ct_report_uri": string
  //
  // The JSON dictionary keys are strings containing
  // Base64(SHA256(TransportSecurityState::CanonicalizeHost(domain))).
  // The reason for hashing them is so that the stored state does not
  // trivially reveal a user's browsing history to an attacker reading the
  // serialized state on disk.
  std::optional<std::string> SerializeData() override;

  // Clears any existing non-static entries, and then re-populates
  // |transport_security_state_|.
  void LoadEntries(const std::string& serialized);

  // Returns the commit interval used by the ImportantFileWriter.
  static base::TimeDelta GetCommitInterval();

 private:
  // Populates |state| from the JSON string |serialized|.
  static void Deserialize(const std::string& serialized,
                          TransportSecurityState* state,
                          bool& contains_legacy_expect_ct_data);

  void CompleteLoad(const std::string& state);
  void OnWriteFinished(base::OnceClosure callback);

  raw_ptr<TransportSecurityState> transport_security_state_;

  // Helper for safely writing the data.
  base::ImportantFileWriter writer_;

  scoped_refptr<base::SequencedTaskRunner> foreground_runner_;
  scoped_refptr<base::SequencedTaskRunner> background_runner_;

  base::WeakPtrFactory<TransportSecurityPersister> weak_ptr_factory_{this};
};

}  // namespace net

#endif  // NET_HTTP_TRANSPORT_SECURITY_PERSISTER_H_
