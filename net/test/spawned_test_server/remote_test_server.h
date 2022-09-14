// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_TEST_SPAWNED_TEST_SERVER_REMOTE_TEST_SERVER_H_
#define NET_TEST_SPAWNED_TEST_SERVER_REMOTE_TEST_SERVER_H_

#include <string>

#include "base/threading/thread.h"
#include "net/test/spawned_test_server/base_test_server.h"

namespace net {

class RemoteTestServerSpawnerRequest;

// The RemoteTestServer runs an external Python-based test server in another
// machine that is different from the machine that executes the tests. It is
// necessary because it's not always possible to run the test server on the same
// machine (it doesn't run on Android and Fuchsia because it's written in
// Python).
//
// The actual test server is executed on the host machine, while the unit tests
// themselves continue running on the device. To control the test server on the
// host machine, a second HTTP server is started, the spawner server, which
// controls the life cycle of remote test servers. Calls to start/kill the
// SpawnedTestServer are then redirected to the spawner server via
// this spawner communicator. The spawner is implemented in
// build/util/lib/common/chrome_test_server_spawner.py .
//
// On Fuchsia, the URL for the spawner server is passed to a test via the
// --remote-test-server-spawner-url-base switch on the command line. On other
// platforms, the URL is discovered by reading config file that's expected to be
// written on the test device by the test scrips. Location of the config
// dependends on platform:
//   - Android: DIR_ANDROID_EXTERNAL_STORAGE/net-test-server-config
//   - other: DIR_TEMP/net-test-server-config
//
// The config file must be stored in the following format:
//   {
//     'spawner_url_base': 'http://localhost:5000'
//   }
//
// 'spawner_url_base' specifies base URL for the spawner.
//
// Currently the following two commands are supported by spawner.
//
// (1) Start Python test server, format is:
// Path: "/start".
// Method: "POST".
// Data to server: all arguments needed to launch the Python test server, in
//   JSON format.
// Data from server: a JSON dict includes the following two field if success,
//   "port": the port the Python test server actually listen on that.
//   "message": must be "started".
//
// (2) Kill Python test server, format is:
// Path: "/kill".
// Method: "GET".
// Data to server: port=<server_port>.
// Data from server: String "killed" returned if success.
//
// The internal I/O thread is required by net stack to perform net I/O.
// The Start/StopServer methods block the caller thread until result is
// fetched from spawner server or timed-out.
class RemoteTestServer : public BaseTestServer {
 public:
  // Initialize a TestServer. |document_root| must be a relative path under the
  // root tree.
  RemoteTestServer(Type type, const base::FilePath& document_root);

  // Initialize a TestServer with a specific set of SSLOptions.
  // |document_root| must be a relative path under the root tree.
  RemoteTestServer(Type type,
                   const SSLOptions& ssl_options,
                   const base::FilePath& document_root);

  RemoteTestServer(const RemoteTestServer&) = delete;
  RemoteTestServer& operator=(const RemoteTestServer&) = delete;

  ~RemoteTestServer() override;

  // BaseTestServer overrides.
  [[nodiscard]] bool StartInBackground() override;
  [[nodiscard]] bool BlockUntilStarted() override;

  // Stops the Python test server that is running on the host machine.
  bool Stop();

  // Returns the actual path of document root for the test cases. This function
  // should be called by test cases to retrieve the actual document root path
  // on the Android device, otherwise document_root() function is used to get
  // the document root.
  base::FilePath GetDocumentRoot() const;

 private:
  bool Init(const base::FilePath& document_root);

  // Returns URL for the specified spawner |command|.
  GURL GetSpawnerUrl(const std::string& command) const;

  // URL of the test server spawner. Read from the config file during
  // initialization.
  std::string spawner_url_base_;

  // Thread used to run all IO activity in RemoteTestServerSpawnerRequest and
  // |ocsp_proxy_|.
  base::Thread io_thread_;

  std::unique_ptr<RemoteTestServerSpawnerRequest> start_request_;

  // Server port. Non-zero when the server is running.
  int remote_port_ = 0;
};

}  // namespace net

#endif  // NET_TEST_SPAWNED_TEST_SERVER_REMOTE_TEST_SERVER_H_
