// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file contains test infrastructure for multiple files
// (current cookie_monster_unittest.cc and cookie_monster_perftest.cc)
// that need to test out CookieMonster interactions with the backing store.
// It should only be included by test code.

#ifndef NET_COOKIES_COOKIE_MONSTER_STORE_TEST_H_
#define NET_COOKIES_COOKIE_MONSTER_STORE_TEST_H_

#include <stdint.h>

#include <map>
#include <memory>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "base/macros.h"
#include "net/cookies/canonical_cookie.h"
#include "net/cookies/cookie_monster.h"
#include "net/log/net_log_with_source.h"

class GURL;

namespace base {
class Time;
}

namespace net {

// Describes a call to one of the 5 functions of PersistentCookieStore.
struct CookieStoreCommand {
  enum Type {
    LOAD,
    LOAD_COOKIES_FOR_KEY,
    // UPDATE_ACCESS_TIME is not included in this list, because get cookie
    // commands may or may not end updating the access time, unless they have
    // the option set not to do so.
    ADD,
    REMOVE,
  };

  // Constructor for LOAD and LOAD_COOKIES_FOR_KEY calls.  |key| should be empty
  // for LOAD_COOKIES_FOR_KEY.
  CookieStoreCommand(
      Type type,
      CookieMonster::PersistentCookieStore::LoadedCallback loaded_callback,
      const std::string& key);

  // Constructor for ADD, UPDATE_ACCESS_TIME, and REMOVE calls.
  CookieStoreCommand(Type type, const CanonicalCookie& cookie);
  CookieStoreCommand(CookieStoreCommand&& other);
  ~CookieStoreCommand();

  Type type;

  // Only non-null for LOAD and LOAD_COOKIES_FOR_KEY.
  CookieMonster::PersistentCookieStore::LoadedCallback loaded_callback;

  // Only non-empty for LOAD_COOKIES_FOR_KEY.
  std::string key;

  // Only non-null for ADD, UPDATE_ACCESS_TIME, and REMOVE.
  CanonicalCookie cookie;
};

// Implementation of PersistentCookieStore that captures the
// received commands and saves them to a list.
// The result of calls to Load() can be configured using SetLoadExpectation().
class MockPersistentCookieStore : public CookieMonster::PersistentCookieStore {
 public:
  typedef std::vector<CookieStoreCommand> CommandList;

  MockPersistentCookieStore();

  // When set, Load() and LoadCookiesForKey() calls are store in the command
  // list, rather than being automatically executed. Defaults to false.
  void set_store_load_commands(bool store_load_commands) {
    store_load_commands_ = store_load_commands;
  }

  void SetLoadExpectation(bool return_value,
                          std::vector<std::unique_ptr<CanonicalCookie>> result);

  const CommandList& commands() const { return commands_; }
  CommandList TakeCommands() { return std::move(commands_); }
  CookieMonster::PersistentCookieStore::LoadedCallback TakeCallbackAt(
      size_t i) {
    return std::move(commands_[i].loaded_callback);
  }

  void Load(LoadedCallback loaded_callback,
            const NetLogWithSource& net_log) override;

  void LoadCookiesForKey(const std::string& key,
                         LoadedCallback loaded_callback) override;

  void AddCookie(const CanonicalCookie& cookie) override;

  void UpdateCookieAccessTime(const CanonicalCookie& cookie) override;

  void DeleteCookie(const CanonicalCookie& cookie) override;

  void SetForceKeepSessionState() override;

  void SetBeforeCommitCallback(base::RepeatingClosure callback) override;

  void Flush(base::OnceClosure callback) override;

 protected:
  ~MockPersistentCookieStore() override;

 private:
  CommandList commands_;

  bool store_load_commands_;

  // Deferred result to use when Load() is called.
  bool load_return_value_;
  std::vector<std::unique_ptr<CanonicalCookie>> load_result_;
  // Indicates if the store has been fully loaded to avoid returning duplicate
  // cookies.
  bool loaded_;

  DISALLOW_COPY_AND_ASSIGN(MockPersistentCookieStore);
};

// Helper to build a single CanonicalCookie.
std::unique_ptr<CanonicalCookie> BuildCanonicalCookie(
    const GURL& url,
    const std::string& cookie_line,
    const base::Time& creation_time);

// Helper to build a list of CanonicalCookie*s.
void AddCookieToList(const GURL& url,
                     const std::string& cookie_line,
                     const base::Time& creation_time,
                     std::vector<std::unique_ptr<CanonicalCookie>>* out_list);

// Just act like a backing database.  Keep cookie information from
// Add/Update/Delete and regurgitate it when Load is called.
class MockSimplePersistentCookieStore
    : public CookieMonster::PersistentCookieStore {
 public:
  MockSimplePersistentCookieStore();

  void Load(LoadedCallback loaded_callback,
            const NetLogWithSource& net_log) override;

  void LoadCookiesForKey(const std::string& key,
                         LoadedCallback loaded_callback) override;

  void AddCookie(const CanonicalCookie& cookie) override;

  void UpdateCookieAccessTime(const CanonicalCookie& cookie) override;

  void DeleteCookie(const CanonicalCookie& cookie) override;

  void SetForceKeepSessionState() override;

  void SetBeforeCommitCallback(base::RepeatingClosure callback) override;

  void Flush(base::OnceClosure callback) override;

 protected:
  ~MockSimplePersistentCookieStore() override;

 private:
  typedef std::map<std::tuple<std::string, std::string, std::string>,
                   CanonicalCookie>
      CanonicalCookieMap;

  CanonicalCookieMap cookies_;

  // Indicates if the store has been fully loaded to avoid return duplicate
  // cookies in subsequent load requests
  bool loaded_;
};

// Helper function for creating a CookieMonster backed by a
// MockSimplePersistentCookieStore for garbage collection testing.
//
// Fill the store through import with |num_*_cookies| cookies,
// |num_old_*_cookies| with access time Now()-days_old, the rest with access
// time Now(). Cookies made by |num_secure_cookies| and |num_non_secure_cookies|
// will be marked secure and non-secure, respectively. Do two SetCookies().
// Return whether each of the two SetCookies() took longer than |gc_perf_micros|
// to complete, and how many cookie were left in the store afterwards.
std::unique_ptr<CookieMonster> CreateMonsterFromStoreForGC(
    int num_secure_cookies,
    int num_old_secure_cookies,
    int num_non_secure_cookies,
    int num_old_non_secure_cookies,
    int days_old);

}  // namespace net

#endif  // NET_COOKIES_COOKIE_MONSTER_STORE_TEST_H_
