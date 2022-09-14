// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_SSL_SSL_KEY_LOGGER_H_
#define NET_SSL_SSL_KEY_LOGGER_H_

#include <memory>
#include <string>

#include "base/no_destructor.h"
#include "net/base/net_export.h"
#include "third_party/boringssl/src/include/openssl/ssl.h"

namespace net {

// SSLKeyLogger logs SSL key material for debugging purposes. This should only
// be used when requested by the user, typically via the SSLKEYLOGFILE
// environment variable. See also
// https://developer.mozilla.org/en-US/docs/Mozilla/Projects/NSS/Key_Log_Format.
class NET_EXPORT SSLKeyLogger {
 public:
  virtual ~SSLKeyLogger() = default;

  // Writes |line| followed by a newline. This may be called by multiple threads
  // simultaneously. If two calls race, the order of the lines is undefined, but
  // each line will be written atomically.
  virtual void WriteLine(const std::string& line) = 0;
};

// SSLKeyLoggerManager owns a single global instance of SSLKeyLogger, allowing
// it to safely be registered on multiple SSL_CTX instances.
class NET_EXPORT SSLKeyLoggerManager {
 public:
  ~SSLKeyLoggerManager() = delete;
  SSLKeyLoggerManager(const SSLKeyLoggerManager&) = delete;
  SSLKeyLoggerManager& operator=(const SSLKeyLoggerManager&) = delete;

  // Returns true if an SSLKeyLogger has been set.
  static bool IsActive();

  // Set the SSLKeyLogger to use.
  static void SetSSLKeyLogger(std::unique_ptr<SSLKeyLogger> logger);

  // Logs |line| to the |logger| that was registered with SetSSLKeyLogger.
  // This function will crash if a logger has not been registered.
  // The function signature allows it to be registered with
  // SSL_CTX_set_keylog_callback, the |ssl| parameter is unused.
  static void KeyLogCallback(const SSL* /*ssl*/, const char* line);

 private:
  friend base::NoDestructor<SSLKeyLoggerManager>;

  SSLKeyLoggerManager();

  // Get the global SSLKeyLoggerManager instance.
  static SSLKeyLoggerManager* Get();

  std::unique_ptr<SSLKeyLogger> ssl_key_logger_;
};

}  // namespace net

#endif  // NET_SSL_SSL_KEY_LOGGER_H_
