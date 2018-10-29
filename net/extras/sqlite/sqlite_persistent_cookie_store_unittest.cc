// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/extras/sqlite/sqlite_persistent_cookie_store.h"

#include <map>
#include <memory>
#include <set>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/location.h"
#include "base/memory/ref_counted.h"
#include "base/run_loop.h"
#include "base/sequenced_task_runner.h"
#include "base/strings/stringprintf.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/post_task.h"
#include "base/test/bind_test_util.h"
#include "base/threading/thread_restrictions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "crypto/encryptor.h"
#include "crypto/symmetric_key.h"
#include "net/base/test_completion_callback.h"
#include "net/cookies/canonical_cookie.h"
#include "net/cookies/cookie_constants.h"
#include "net/cookies/cookie_store_test_callbacks.h"
#include "net/extras/sqlite/cookie_crypto_delegate.h"
#include "net/log/net_log_capture_mode.h"
#include "net/log/test_net_log.h"
#include "net/log/test_net_log_util.h"
#include "net/test/test_with_scoped_task_environment.h"
#include "sql/database.h"
#include "sql/meta_table.h"
#include "sql/statement.h"
#include "sql/transaction.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace net {

namespace {

const base::FilePath::CharType kCookieFilename[] = FILE_PATH_LITERAL("Cookies");

class CookieCryptor : public CookieCryptoDelegate {
 public:
  CookieCryptor();
  bool ShouldEncrypt() override;
  bool EncryptString(const std::string& plaintext,
                     std::string* ciphertext) override;
  bool DecryptString(const std::string& ciphertext,
                     std::string* plaintext) override;

  bool should_encrypt_;

 private:
  std::unique_ptr<crypto::SymmetricKey> key_;
  crypto::Encryptor encryptor_;
};

CookieCryptor::CookieCryptor()
    : should_encrypt_(true),
      key_(crypto::SymmetricKey::DeriveKeyFromPasswordUsingPbkdf2(
          crypto::SymmetricKey::AES,
          "password",
          "saltiest",
          1000,
          256)) {
  std::string iv("the iv: 16 bytes");
  encryptor_.Init(key_.get(), crypto::Encryptor::CBC, iv);
}

bool CookieCryptor::ShouldEncrypt() {
  return should_encrypt_;
}

bool CookieCryptor::EncryptString(const std::string& plaintext,
                                  std::string* ciphertext) {
  return encryptor_.Encrypt(plaintext, ciphertext);
}

bool CookieCryptor::DecryptString(const std::string& ciphertext,
                                  std::string* plaintext) {
  return encryptor_.Decrypt(ciphertext, plaintext);
}

}  // namespace

typedef std::vector<std::unique_ptr<CanonicalCookie>> CanonicalCookieVector;

class SQLitePersistentCookieStoreTest : public TestWithScopedTaskEnvironment {
 public:
  SQLitePersistentCookieStoreTest()
      : loaded_event_(base::WaitableEvent::ResetPolicy::AUTOMATIC,
                      base::WaitableEvent::InitialState::NOT_SIGNALED),
        db_thread_event_(base::WaitableEvent::ResetPolicy::AUTOMATIC,
                         base::WaitableEvent::InitialState::NOT_SIGNALED) {}

  void OnLoaded(CanonicalCookieVector cookies) {
    cookies_.swap(cookies);
    loaded_event_.Signal();
  }

  void OnKeyLoaded(base::OnceClosure closure, CanonicalCookieVector cookies) {
    cookies_.swap(cookies);
    std::move(closure).Run();
  }

  void Load(CanonicalCookieVector* cookies) {
    EXPECT_FALSE(loaded_event_.IsSignaled());
    store_->Load(base::Bind(&SQLitePersistentCookieStoreTest::OnLoaded,
                            base::Unretained(this)),
                 net_log_.bound());
    loaded_event_.Wait();
    cookies->swap(cookies_);
  }

  void Flush() {
    base::WaitableEvent event(base::WaitableEvent::ResetPolicy::AUTOMATIC,
                              base::WaitableEvent::InitialState::NOT_SIGNALED);
    store_->Flush(
        base::Bind(&base::WaitableEvent::Signal, base::Unretained(&event)));
    event.Wait();
  }

  void DestroyStore() {
    store_ = nullptr;
    // Make sure we wait until the destructor has run by running all
    // ScopedTaskEnvironment tasks.
    RunUntilIdle();
  }

  void Create(bool crypt_cookies,
              bool restore_old_session_cookies,
              bool use_current_thread) {
    if (crypt_cookies)
      cookie_crypto_delegate_.reset(new CookieCryptor());

    store_ = new SQLitePersistentCookieStore(
        temp_dir_.GetPath().Append(kCookieFilename),
        use_current_thread ? base::ThreadTaskRunnerHandle::Get()
                           : client_task_runner_,
        background_task_runner_, restore_old_session_cookies,
        cookie_crypto_delegate_.get());
  }

  void CreateAndLoad(bool crypt_cookies,
                     bool restore_old_session_cookies,
                     CanonicalCookieVector* cookies) {
    Create(crypt_cookies, restore_old_session_cookies,
           false /* use_current_thread */);
    Load(cookies);
  }

  void InitializeStore(bool crypt, bool restore_old_session_cookies) {
    CanonicalCookieVector cookies;
    CreateAndLoad(crypt, restore_old_session_cookies, &cookies);
    EXPECT_EQ(0U, cookies.size());
  }

  void WaitOnDBEvent() {
    base::ScopedAllowBaseSyncPrimitivesForTesting allow_base_sync_primitives;
    db_thread_event_.Wait();
  }

  // Adds a persistent cookie to store_.
  void AddCookie(const std::string& name,
                 const std::string& value,
                 const std::string& domain,
                 const std::string& path,
                 const base::Time& creation) {
    store_->AddCookie(CanonicalCookie(
        name, value, domain, path, creation, creation, base::Time(), false,
        false, CookieSameSite::DEFAULT_MODE, COOKIE_PRIORITY_DEFAULT));
  }

  void AddCookieWithExpiration(const std::string& name,
                               const std::string& value,
                               const std::string& domain,
                               const std::string& path,
                               const base::Time& creation,
                               const base::Time& expiration) {
    store_->AddCookie(CanonicalCookie(
        name, value, domain, path, creation, expiration, base::Time(), false,
        false, CookieSameSite::DEFAULT_MODE, COOKIE_PRIORITY_DEFAULT));
  }

  std::string ReadRawDBContents() {
    std::string contents;
    if (!base::ReadFileToString(temp_dir_.GetPath().Append(kCookieFilename),
                                &contents))
      return std::string();
    return contents;
  }

  void SetUp() override { ASSERT_TRUE(temp_dir_.CreateUniqueTempDir()); }

  void TearDown() override {
    DestroyStore();
  }

 protected:
  const scoped_refptr<base::SequencedTaskRunner> background_task_runner_ =
      base::CreateSequencedTaskRunnerWithTraits({base::MayBlock()});
  const scoped_refptr<base::SequencedTaskRunner> client_task_runner_ =
      base::CreateSequencedTaskRunnerWithTraits({base::MayBlock()});
  base::WaitableEvent loaded_event_;
  base::WaitableEvent db_thread_event_;
  CanonicalCookieVector cookies_;
  base::ScopedTempDir temp_dir_;
  scoped_refptr<SQLitePersistentCookieStore> store_;
  std::unique_ptr<CookieCryptor> cookie_crypto_delegate_;
  BoundTestNetLog net_log_;
};

TEST_F(SQLitePersistentCookieStoreTest, TestInvalidMetaTableRecovery) {
  InitializeStore(false, false);
  AddCookie("A", "B", "foo.bar", "/", base::Time::Now());
  DestroyStore();

  // Load up the store and verify that it has good data in it.
  CanonicalCookieVector cookies;
  CreateAndLoad(false, false, &cookies);
  ASSERT_EQ(1U, cookies.size());
  ASSERT_STREQ("foo.bar", cookies[0]->Domain().c_str());
  ASSERT_STREQ("A", cookies[0]->Name().c_str());
  ASSERT_STREQ("B", cookies[0]->Value().c_str());
  DestroyStore();
  cookies.clear();

  // Now corrupt the meta table.
  {
    sql::Database db;
    ASSERT_TRUE(db.Open(temp_dir_.GetPath().Append(kCookieFilename)));
    sql::MetaTable meta_table_;
    meta_table_.Init(&db, 1, 1);
    ASSERT_TRUE(db.Execute("DELETE FROM meta"));
    db.Close();
  }

  // Upon loading, the database should be reset to a good, blank state.
  CreateAndLoad(false, false, &cookies);
  ASSERT_EQ(0U, cookies.size());

  // Verify that, after, recovery, the database persists properly.
  AddCookie("X", "Y", "foo.bar", "/", base::Time::Now());
  DestroyStore();
  CreateAndLoad(false, false, &cookies);
  ASSERT_EQ(1U, cookies.size());
  ASSERT_STREQ("foo.bar", cookies[0]->Domain().c_str());
  ASSERT_STREQ("X", cookies[0]->Name().c_str());
  ASSERT_STREQ("Y", cookies[0]->Value().c_str());
  cookies.clear();
}

// Test if data is stored as expected in the SQLite database.
TEST_F(SQLitePersistentCookieStoreTest, TestPersistance) {
  InitializeStore(false, false);
  AddCookie("A", "B", "foo.bar", "/", base::Time::Now());
  // Replace the store effectively destroying the current one and forcing it
  // to write its data to disk. Then we can see if after loading it again it
  // is still there.
  DestroyStore();
  // Reload and test for persistence
  CanonicalCookieVector cookies;
  CreateAndLoad(false, false, &cookies);
  ASSERT_EQ(1U, cookies.size());
  ASSERT_STREQ("foo.bar", cookies[0]->Domain().c_str());
  ASSERT_STREQ("A", cookies[0]->Name().c_str());
  ASSERT_STREQ("B", cookies[0]->Value().c_str());

  // Now delete the cookie and check persistence again.
  store_->DeleteCookie(*cookies[0]);
  DestroyStore();
  cookies.clear();

  // Reload and check if the cookie has been removed.
  CreateAndLoad(false, false, &cookies);
  ASSERT_EQ(0U, cookies.size());
}

TEST_F(SQLitePersistentCookieStoreTest, TestSessionCookiesDeletedOnStartup) {
  // Initialize the cookie store with 3 persistent cookies, 5 transient
  // cookies.
  InitializeStore(false, false);

  // Add persistent cookies.
  base::Time t = base::Time::Now();
  AddCookie("A", "B", "a1.com", "/", t);
  t += base::TimeDelta::FromMicroseconds(10);
  AddCookie("A", "B", "a2.com", "/", t);
  t += base::TimeDelta::FromMicroseconds(10);
  AddCookie("A", "B", "a3.com", "/", t);

  // Add transient cookies.
  t += base::TimeDelta::FromMicroseconds(10);
  AddCookieWithExpiration("A", "B", "b1.com", "/", t, base::Time());
  t += base::TimeDelta::FromMicroseconds(10);
  AddCookieWithExpiration("A", "B", "b2.com", "/", t, base::Time());
  t += base::TimeDelta::FromMicroseconds(10);
  AddCookieWithExpiration("A", "B", "b3.com", "/", t, base::Time());
  t += base::TimeDelta::FromMicroseconds(10);
  AddCookieWithExpiration("A", "B", "b4.com", "/", t, base::Time());
  t += base::TimeDelta::FromMicroseconds(10);
  AddCookieWithExpiration("A", "B", "b5.com", "/", t, base::Time());
  DestroyStore();

  // Load the store a second time. Before the store finishes loading, add a
  // transient cookie and flush it to disk.
  store_ = new SQLitePersistentCookieStore(
      temp_dir_.GetPath().Append(kCookieFilename), client_task_runner_,
      background_task_runner_, false, nullptr);

  // Posting a blocking task to db_thread_ makes sure that the DB thread waits
  // until both Load and Flush have been posted to its task queue.
  background_task_runner_->PostTask(
      FROM_HERE, base::Bind(&SQLitePersistentCookieStoreTest::WaitOnDBEvent,
                            base::Unretained(this)));
  store_->Load(base::Bind(&SQLitePersistentCookieStoreTest::OnLoaded,
                          base::Unretained(this)),
               NetLogWithSource());
  t += base::TimeDelta::FromMicroseconds(10);
  AddCookieWithExpiration("A", "B", "c.com", "/", t, base::Time());
  base::WaitableEvent event(base::WaitableEvent::ResetPolicy::AUTOMATIC,
                            base::WaitableEvent::InitialState::NOT_SIGNALED);
  store_->Flush(
      base::Bind(&base::WaitableEvent::Signal, base::Unretained(&event)));

  // Now the DB-thread queue contains:
  // (active:)
  // 1. Wait (on db_event)
  // (pending:)
  // 2. "Init And Chain-Load First Domain"
  // 3. Add Cookie (c.com)
  // 4. Flush Cookie (c.com)
  db_thread_event_.Signal();
  event.Wait();
  loaded_event_.Wait();
  cookies_.clear();
  DestroyStore();

  // Load the store a third time, this time restoring session cookies. The
  // store should contain exactly 4 cookies: the 3 persistent, and "c.com",
  // which was added during the second cookie store load.
  store_ = new SQLitePersistentCookieStore(
      temp_dir_.GetPath().Append(kCookieFilename), client_task_runner_,
      background_task_runner_, true, nullptr);
  store_->Load(base::Bind(&SQLitePersistentCookieStoreTest::OnLoaded,
                          base::Unretained(this)),
               NetLogWithSource());
  loaded_event_.Wait();
  ASSERT_EQ(4u, cookies_.size());
  cookies_.clear();
}

// Test that priority load of cookies for a specfic domain key could be
// completed before the entire store is loaded
TEST_F(SQLitePersistentCookieStoreTest, TestLoadCookiesForKey) {
  InitializeStore(false, false);
  base::Time t = base::Time::Now();
  AddCookie("A", "B", "foo.bar", "/", t);
  t += base::TimeDelta::FromMicroseconds(10);
  AddCookie("A", "B", "www.aaa.com", "/", t);
  t += base::TimeDelta::FromMicroseconds(10);
  AddCookie("A", "B", "travel.aaa.com", "/", t);
  t += base::TimeDelta::FromMicroseconds(10);
  AddCookie("A", "B", "www.bbb.com", "/", t);
  DestroyStore();

  // base::test::ScopedTaskEnvironment runs |background_task_runner_| and
  // |client_task_runner_| on the same thread. Therefore, when a
  // |background_task_runner_| task is blocked, |client_task_runner_| tasks
  // can't run. To allow precise control of |background_task_runner_| without
  // preventing client tasks to run, use base::ThreadTaskRunnerHandle::Get()
  // instead of |client_task_runner_| for this test.
  Create(false /* crypt_cookies */, false /* restore_old_session_cookies */,
         true /* use_current_thread */);

  // Posting a blocking task to db_thread_ makes sure that the DB thread waits
  // until both Load and LoadCookiesForKey have been posted to its task queue.
  background_task_runner_->PostTask(
      FROM_HERE, base::Bind(&SQLitePersistentCookieStoreTest::WaitOnDBEvent,
                            base::Unretained(this)));
  BoundTestNetLog net_log;
  store_->Load(base::Bind(&SQLitePersistentCookieStoreTest::OnLoaded,
                          base::Unretained(this)),
               net_log.bound());
  base::RunLoop run_loop;
  net_log.SetCaptureMode(NetLogCaptureMode::Default());
  store_->LoadCookiesForKey(
      "aaa.com", base::Bind(&SQLitePersistentCookieStoreTest::OnKeyLoaded,
                            base::Unretained(this), run_loop.QuitClosure()));
  background_task_runner_->PostTask(
      FROM_HERE, base::Bind(&SQLitePersistentCookieStoreTest::WaitOnDBEvent,
                            base::Unretained(this)));

  // Now the DB-thread queue contains:
  // (active:)
  // 1. Wait (on db_event)
  // (pending:)
  // 2. "Init And Chain-Load First Domain"
  // 3. Priority Load (aaa.com)
  // 4. Wait (on db_event)
  db_thread_event_.Signal();

  // Wait until the OnKeyLoaded callback has run.
  run_loop.Run();
  EXPECT_FALSE(loaded_event_.IsSignaled());

  std::set<std::string> cookies_loaded;
  for (CanonicalCookieVector::const_iterator it = cookies_.begin();
       it != cookies_.end(); ++it) {
    cookies_loaded.insert((*it)->Domain().c_str());
  }
  cookies_.clear();
  ASSERT_GT(4U, cookies_loaded.size());
  ASSERT_EQ(true, cookies_loaded.find("www.aaa.com") != cookies_loaded.end());
  ASSERT_EQ(true,
            cookies_loaded.find("travel.aaa.com") != cookies_loaded.end());

  db_thread_event_.Signal();

  RunUntilIdle();
  EXPECT_TRUE(loaded_event_.IsSignaled());

  for (CanonicalCookieVector::const_iterator it = cookies_.begin();
       it != cookies_.end(); ++it) {
    cookies_loaded.insert((*it)->Domain().c_str());
  }
  ASSERT_EQ(4U, cookies_loaded.size());
  ASSERT_EQ(cookies_loaded.find("foo.bar") != cookies_loaded.end(), true);
  ASSERT_EQ(cookies_loaded.find("www.bbb.com") != cookies_loaded.end(), true);
  cookies_.clear();

  store_ = nullptr;
  TestNetLogEntry::List entries;
  net_log.GetEntries(&entries);
  size_t pos = ExpectLogContainsSomewhere(
      entries, 0, NetLogEventType::COOKIE_PERSISTENT_STORE_LOAD,
      NetLogEventPhase::BEGIN);
  pos = ExpectLogContainsSomewhere(
      entries, pos, NetLogEventType::COOKIE_PERSISTENT_STORE_LOAD,
      NetLogEventPhase::END);
  pos = ExpectLogContainsSomewhere(
      entries, 0, NetLogEventType::COOKIE_PERSISTENT_STORE_LOAD,
      NetLogEventPhase::BEGIN);
  pos = ExpectLogContainsSomewhere(
      entries, pos, NetLogEventType::COOKIE_PERSISTENT_STORE_KEY_LOAD_STARTED,
      NetLogEventPhase::NONE);
  std::string key;
  EXPECT_FALSE(entries[pos].GetStringValue("key", &key));
  pos = ExpectLogContainsSomewhere(
      entries, pos, NetLogEventType::COOKIE_PERSISTENT_STORE_KEY_LOAD_COMPLETED,
      NetLogEventPhase::NONE);
  pos = ExpectLogContainsSomewhere(
      entries, pos, NetLogEventType::COOKIE_PERSISTENT_STORE_LOAD,
      NetLogEventPhase::END);
  ExpectLogContainsSomewhere(entries, pos,
                             NetLogEventType::COOKIE_PERSISTENT_STORE_CLOSED,
                             NetLogEventPhase::NONE);
}

TEST_F(SQLitePersistentCookieStoreTest, TestBeforeFlushCallback) {
  InitializeStore(false, false);

  struct Counter {
    int count = 0;
    void increment() { count++; }
  };

  Counter counter;
  store_->SetBeforeFlushCallback(
      base::Bind(&Counter::increment, base::Unretained(&counter)));

  // The implementation of SQLitePersistentCookieStore::Backend flushes changes
  // after 30s or 512 pending operations. Add 512 cookies to the store to test
  // that the callback gets called when SQLitePersistentCookieStore internally
  // flushes its store.
  for (int i = 0; i < 512; i++) {
    // Each cookie needs a unique timestamp for creation_utc (see DB schema).
    base::Time t = base::Time::Now() + base::TimeDelta::FromMicroseconds(i);
    AddCookie(base::StringPrintf("%d", i), "foo", "example.com", "/", t);
  }

  RunUntilIdle();
  EXPECT_GT(counter.count, 0);

  DestroyStore();
}

// Test that we can force the database to be written by calling Flush().
TEST_F(SQLitePersistentCookieStoreTest, TestFlush) {
  InitializeStore(false, false);
  // File timestamps don't work well on all platforms, so we'll determine
  // whether the DB file has been modified by checking its size.
  base::FilePath path = temp_dir_.GetPath().Append(kCookieFilename);
  base::File::Info info;
  ASSERT_TRUE(base::GetFileInfo(path, &info));
  int64_t base_size = info.size;

  // Write some large cookies, so the DB will have to expand by several KB.
  for (char c = 'a'; c < 'z'; ++c) {
    // Each cookie needs a unique timestamp for creation_utc (see DB schema).
    base::Time t = base::Time::Now() + base::TimeDelta::FromMicroseconds(c);
    std::string name(1, c);
    std::string value(1000, c);
    AddCookie(name, value, "foo.bar", "/", t);
  }

  Flush();

  // We forced a write, so now the file will be bigger.
  ASSERT_TRUE(base::GetFileInfo(path, &info));
  ASSERT_GT(info.size, base_size);
}

// Test loading old session cookies from the disk.
TEST_F(SQLitePersistentCookieStoreTest, TestLoadOldSessionCookies) {
  InitializeStore(false, true);

  // Add a session cookie.
  store_->AddCookie(
      CanonicalCookie("C", "D", "sessioncookie.com", "/", base::Time::Now(),
                      base::Time(), base::Time(), false, false,
                      CookieSameSite::DEFAULT_MODE, COOKIE_PRIORITY_DEFAULT));

  // Force the store to write its data to the disk.
  DestroyStore();

  // Create a store that loads session cookies and test that the session cookie
  // was loaded.
  CanonicalCookieVector cookies;
  CreateAndLoad(false, true, &cookies);

  ASSERT_EQ(1U, cookies.size());
  ASSERT_STREQ("sessioncookie.com", cookies[0]->Domain().c_str());
  ASSERT_STREQ("C", cookies[0]->Name().c_str());
  ASSERT_STREQ("D", cookies[0]->Value().c_str());
  ASSERT_EQ(COOKIE_PRIORITY_DEFAULT, cookies[0]->Priority());

  cookies.clear();
}

// Test refusing to load old session cookies from the disk.
TEST_F(SQLitePersistentCookieStoreTest, TestDontLoadOldSessionCookies) {
  InitializeStore(false, true);

  // Add a session cookie.
  store_->AddCookie(
      CanonicalCookie("C", "D", "sessioncookie.com", "/", base::Time::Now(),
                      base::Time(), base::Time(), false, false,
                      CookieSameSite::DEFAULT_MODE, COOKIE_PRIORITY_DEFAULT));

  // Force the store to write its data to the disk.
  DestroyStore();

  // Create a store that doesn't load old session cookies and test that the
  // session cookie was not loaded.
  CanonicalCookieVector cookies;
  CreateAndLoad(false, false, &cookies);
  ASSERT_EQ(0U, cookies.size());

  // The store should also delete the session cookie. Wait until that has been
  // done.
  DestroyStore();

  // Create a store that loads old session cookies and test that the session
  // cookie is gone.
  CreateAndLoad(false, true, &cookies);
  ASSERT_EQ(0U, cookies.size());
}

// Confirm bad cookies on disk don't get looaded, and that we also remove them
// from the database.
TEST_F(SQLitePersistentCookieStoreTest, FilterBadCookiesAndFixupDb) {
  // Create an on-disk store.
  InitializeStore(false, true);
  DestroyStore();

  // Add some cookies in by hand.
  base::FilePath store_name(temp_dir_.GetPath().Append(kCookieFilename));
  std::unique_ptr<sql::Database> db(std::make_unique<sql::Database>());
  ASSERT_TRUE(db->Open(store_name));
  sql::Statement stmt(db->GetUniqueStatement(
      "INSERT INTO cookies (creation_utc, host_key, name, value, "
      "encrypted_value, path, expires_utc, is_secure, is_httponly, "
      "firstpartyonly, last_access_utc, has_expires, is_persistent, priority) "
      "VALUES (?,?,?,?,'',?,0,0,0,0,0,1,1,0)"));
  ASSERT_TRUE(stmt.is_valid());

  struct CookieInfo {
    const char* domain;
    const char* name;
    const char* value;
    const char* path;
  } cookies_info[] = {// A couple non-canonical cookies.
                      {"google.izzle", "A=", "B", "/path"},
                      {"google.izzle", "C ", "D", "/path"},

                      // A canonical cookie for same eTLD+1. This one will get
                      // dropped out of precaution to avoid confusing the site,
                      // even though there is nothing wrong with it.
                      {"sub.google.izzle", "E", "F", "/path"},

                      // A canonical cookie for another eTLD+1
                      {"chromium.org", "G", "H", "/dir"}};

  int64_t creation_time = 1;
  for (auto& cookie_info : cookies_info) {
    stmt.Reset(true);

    stmt.BindInt64(0, creation_time++);
    stmt.BindString(1, cookie_info.domain);
    stmt.BindString(2, cookie_info.name);
    stmt.BindString(3, cookie_info.value);
    stmt.BindString(4, cookie_info.path);
    ASSERT_TRUE(stmt.Run());
  }
  stmt.Clear();
  db.reset();

  // Reopen the store and confirm that the only cookie loaded is the
  // canonical one on an unrelated domain.
  CanonicalCookieVector cookies;
  CreateAndLoad(false, false, &cookies);
  ASSERT_EQ(1U, cookies.size());
  EXPECT_STREQ("chromium.org", cookies[0]->Domain().c_str());
  EXPECT_STREQ("G", cookies[0]->Name().c_str());
  EXPECT_STREQ("H", cookies[0]->Value().c_str());
  EXPECT_STREQ("/dir", cookies[0]->Path().c_str());
  DestroyStore();

  // Make sure that we only have one row left.
  db = std::make_unique<sql::Database>();
  ASSERT_TRUE(db->Open(store_name));
  sql::Statement verify_stmt(db->GetUniqueStatement("SELECT * FROM COOKIES"));
  ASSERT_TRUE(verify_stmt.is_valid());
  int found = 0;
  while (verify_stmt.Step()) {
    ++found;
  }
  EXPECT_TRUE(verify_stmt.Succeeded());
  EXPECT_EQ(1, found);
}

TEST_F(SQLitePersistentCookieStoreTest, PersistIsPersistent) {
  InitializeStore(false, true);
  static const char kSessionName[] = "session";
  static const char kPersistentName[] = "persistent";

  // Add a session cookie.
  store_->AddCookie(CanonicalCookie(
      kSessionName, "val", "sessioncookie.com", "/", base::Time::Now(),
      base::Time(), base::Time(), false, false, CookieSameSite::DEFAULT_MODE,
      COOKIE_PRIORITY_DEFAULT));
  // Add a persistent cookie.
  store_->AddCookie(CanonicalCookie(
      kPersistentName, "val", "sessioncookie.com", "/",
      base::Time::Now() - base::TimeDelta::FromDays(1),
      base::Time::Now() + base::TimeDelta::FromDays(1), base::Time(), false,
      false, CookieSameSite::DEFAULT_MODE, COOKIE_PRIORITY_DEFAULT));

  // Force the store to write its data to the disk.
  DestroyStore();

  // Create a store that loads session cookie and test that the IsPersistent
  // attribute is restored.
  CanonicalCookieVector cookies;
  CreateAndLoad(false, true, &cookies);
  ASSERT_EQ(2U, cookies.size());

  std::map<std::string, CanonicalCookie*> cookie_map;
  for (const auto& cookie : cookies)
    cookie_map[cookie->Name()] = cookie.get();

  auto it = cookie_map.find(kSessionName);
  ASSERT_TRUE(it != cookie_map.end());
  EXPECT_FALSE(cookie_map[kSessionName]->IsPersistent());

  it = cookie_map.find(kPersistentName);
  ASSERT_TRUE(it != cookie_map.end());
  EXPECT_TRUE(cookie_map[kPersistentName]->IsPersistent());

  cookies.clear();
}

TEST_F(SQLitePersistentCookieStoreTest, PriorityIsPersistent) {
  static const char kDomain[] = "sessioncookie.com";
  static const char kLowName[] = "low";
  static const char kMediumName[] = "medium";
  static const char kHighName[] = "high";
  static const char kCookieValue[] = "value";
  static const char kCookiePath[] = "/";

  InitializeStore(false, true);

  // Add a low-priority persistent cookie.
  store_->AddCookie(CanonicalCookie(
      kLowName, kCookieValue, kDomain, kCookiePath,
      base::Time::Now() - base::TimeDelta::FromMinutes(1),
      base::Time::Now() + base::TimeDelta::FromDays(1), base::Time(), false,
      false, CookieSameSite::DEFAULT_MODE, COOKIE_PRIORITY_LOW));

  // Add a medium-priority persistent cookie.
  store_->AddCookie(CanonicalCookie(
      kMediumName, kCookieValue, kDomain, kCookiePath,
      base::Time::Now() - base::TimeDelta::FromMinutes(2),
      base::Time::Now() + base::TimeDelta::FromDays(1), base::Time(), false,
      false, CookieSameSite::DEFAULT_MODE, COOKIE_PRIORITY_MEDIUM));

  // Add a high-priority peristent cookie.
  store_->AddCookie(CanonicalCookie(
      kHighName, kCookieValue, kDomain, kCookiePath,
      base::Time::Now() - base::TimeDelta::FromMinutes(3),
      base::Time::Now() + base::TimeDelta::FromDays(1), base::Time(), false,
      false, CookieSameSite::DEFAULT_MODE, COOKIE_PRIORITY_HIGH));

  // Force the store to write its data to the disk.
  DestroyStore();

  // Create a store that loads session cookie and test that the priority
  // attribute values are restored.
  CanonicalCookieVector cookies;
  CreateAndLoad(false, true, &cookies);
  ASSERT_EQ(3U, cookies.size());

  // Put the cookies into a map, by name, so we can easily find them.
  std::map<std::string, CanonicalCookie*> cookie_map;
  for (const auto& cookie : cookies)
    cookie_map[cookie->Name()] = cookie.get();

  // Validate that each cookie has the correct priority.
  auto it = cookie_map.find(kLowName);
  ASSERT_TRUE(it != cookie_map.end());
  EXPECT_EQ(COOKIE_PRIORITY_LOW, cookie_map[kLowName]->Priority());

  it = cookie_map.find(kMediumName);
  ASSERT_TRUE(it != cookie_map.end());
  EXPECT_EQ(COOKIE_PRIORITY_MEDIUM, cookie_map[kMediumName]->Priority());

  it = cookie_map.find(kHighName);
  ASSERT_TRUE(it != cookie_map.end());
  EXPECT_EQ(COOKIE_PRIORITY_HIGH, cookie_map[kHighName]->Priority());

  cookies.clear();
}

TEST_F(SQLitePersistentCookieStoreTest, SameSiteIsPersistent) {
  const char kDomain[] = "sessioncookie.com";
  const char kNoneName[] = "none";
  const char kLaxName[] = "lax";
  const char kStrictName[] = "strict";
  const char kCookieValue[] = "value";
  const char kCookiePath[] = "/";

  InitializeStore(false, true);

  // Add a non-samesite cookie.
  store_->AddCookie(CanonicalCookie(
      kNoneName, kCookieValue, kDomain, kCookiePath,
      base::Time::Now() - base::TimeDelta::FromMinutes(1),
      base::Time::Now() + base::TimeDelta::FromDays(1), base::Time(), false,
      false, CookieSameSite::NO_RESTRICTION, COOKIE_PRIORITY_DEFAULT));

  // Add a lax-samesite persistent cookie.
  store_->AddCookie(CanonicalCookie(
      kLaxName, kCookieValue, kDomain, kCookiePath,
      base::Time::Now() - base::TimeDelta::FromMinutes(2),
      base::Time::Now() + base::TimeDelta::FromDays(1), base::Time(), false,
      false, CookieSameSite::LAX_MODE, COOKIE_PRIORITY_DEFAULT));

  // Add a strict-samesite persistent cookie.
  store_->AddCookie(CanonicalCookie(
      kStrictName, kCookieValue, kDomain, kCookiePath,
      base::Time::Now() - base::TimeDelta::FromMinutes(3),
      base::Time::Now() + base::TimeDelta::FromDays(1), base::Time(), false,
      false, CookieSameSite::STRICT_MODE, COOKIE_PRIORITY_DEFAULT));

  // Force the store to write its data to the disk.
  DestroyStore();

  // Create a store that loads session cookie and test that the priority
  // attribute values are restored.
  CanonicalCookieVector cookies;
  CreateAndLoad(false, true, &cookies);
  ASSERT_EQ(3U, cookies.size());

  // Put the cookies into a map, by name, for comparison below.
  std::map<std::string, CanonicalCookie*> cookie_map;
  for (const auto& cookie : cookies)
    cookie_map[cookie->Name()] = cookie.get();

  // Validate that each cookie has the correct SameSite.
  ASSERT_EQ(1u, cookie_map.count(kNoneName));
  EXPECT_EQ(CookieSameSite::NO_RESTRICTION, cookie_map[kNoneName]->SameSite());

  ASSERT_EQ(1u, cookie_map.count(kLaxName));
  EXPECT_EQ(CookieSameSite::LAX_MODE, cookie_map[kLaxName]->SameSite());

  ASSERT_EQ(1u, cookie_map.count(kStrictName));
  EXPECT_EQ(CookieSameSite::STRICT_MODE, cookie_map[kStrictName]->SameSite());

  cookies.clear();
}

TEST_F(SQLitePersistentCookieStoreTest, UpdateToEncryption) {
  CanonicalCookieVector cookies;

  // Create unencrypted cookie store and write something to it.
  InitializeStore(false, false);
  AddCookie("name", "value123XYZ", "foo.bar", "/", base::Time::Now());
  DestroyStore();

  // Verify that "value" is visible in the file.  This is necessary in order to
  // have confidence in a later test that "encrypted_value" is not visible.
  std::string contents = ReadRawDBContents();
  EXPECT_NE(0U, contents.length());
  EXPECT_NE(contents.find("value123XYZ"), std::string::npos);

  // Create encrypted cookie store and ensure old cookie still reads.
  cookies.clear();
  EXPECT_EQ(0U, cookies.size());
  CreateAndLoad(true, false, &cookies);
  EXPECT_EQ(1U, cookies.size());
  EXPECT_EQ("name", cookies[0]->Name());
  EXPECT_EQ("value123XYZ", cookies[0]->Value());

  // Make sure we can update existing cookie and add new cookie as encrypted.
  store_->DeleteCookie(*(cookies[0]));
  AddCookie("name", "encrypted_value123XYZ", "foo.bar", "/", base::Time::Now());
  AddCookie("other", "something456ABC", "foo.bar", "/",
            base::Time::Now() + base::TimeDelta::FromMicroseconds(10));
  DestroyStore();
  cookies.clear();
  CreateAndLoad(true, false, &cookies);
  EXPECT_EQ(2U, cookies.size());
  CanonicalCookie* cookie_name = nullptr;
  CanonicalCookie* cookie_other = nullptr;
  if (cookies[0]->Name() == "name") {
    cookie_name = cookies[0].get();
    cookie_other = cookies[1].get();
  } else {
    cookie_name = cookies[1].get();
    cookie_other = cookies[0].get();
  }
  EXPECT_EQ("encrypted_value123XYZ", cookie_name->Value());
  EXPECT_EQ("something456ABC", cookie_other->Value());
  DestroyStore();
  cookies.clear();

  // Examine the real record to make sure plaintext version doesn't exist.
  sql::Database db;
  sql::Statement smt;
  int resultcount = 0;
  ASSERT_TRUE(db.Open(temp_dir_.GetPath().Append(kCookieFilename)));
  smt.Assign(db.GetCachedStatement(SQL_FROM_HERE,
                                   "SELECT * "
                                   "FROM cookies "
                                   "WHERE host_key = 'foo.bar'"));
  while (smt.Step()) {
    resultcount++;
    for (int i = 0; i < smt.ColumnCount(); i++) {
      EXPECT_EQ(smt.ColumnString(i).find("value"), std::string::npos);
      EXPECT_EQ(smt.ColumnString(i).find("something"), std::string::npos);
    }
  }
  EXPECT_EQ(2, resultcount);

  // Verify that "encrypted_value" is NOT visible in the file.
  contents = ReadRawDBContents();
  EXPECT_NE(0U, contents.length());
  EXPECT_EQ(contents.find("encrypted_value123XYZ"), std::string::npos);
  EXPECT_EQ(contents.find("something456ABC"), std::string::npos);
}

TEST_F(SQLitePersistentCookieStoreTest, UpdateFromEncryption) {
  CanonicalCookieVector cookies;

  // Create unencrypted cookie store and write something to it.
  InitializeStore(true, false);
  AddCookie("name", "value123XYZ", "foo.bar", "/", base::Time::Now());
  DestroyStore();

  // Verify that "value" is not visible in the file.
  std::string contents = ReadRawDBContents();
  EXPECT_NE(0U, contents.length());
  EXPECT_EQ(contents.find("value123XYZ"), std::string::npos);

  // Create encrypted cookie store and ensure old cookie still reads.
  cookies.clear();
  EXPECT_EQ(0U, cookies.size());
  CreateAndLoad(true, false, &cookies);
  EXPECT_EQ(1U, cookies.size());
  EXPECT_EQ("name", cookies[0]->Name());
  EXPECT_EQ("value123XYZ", cookies[0]->Value());

  // Make sure we can update existing cookie and it writes unencrypted.
  cookie_crypto_delegate_->should_encrypt_ = false;
  store_->DeleteCookie(*(cookies[0]));
  AddCookie("name", "plaintext_value123XYZ", "foo.bar", "/", base::Time::Now());
  AddCookie("other", "something456ABC", "foo.bar", "/",
            base::Time::Now() + base::TimeDelta::FromMicroseconds(10));
  DestroyStore();
  cookies.clear();
  CreateAndLoad(true, false, &cookies);
  EXPECT_EQ(2U, cookies.size());
  CanonicalCookie* cookie_name = nullptr;
  CanonicalCookie* cookie_other = nullptr;
  if (cookies[0]->Name() == "name") {
    cookie_name = cookies[0].get();
    cookie_other = cookies[1].get();
  } else {
    cookie_name = cookies[1].get();
    cookie_other = cookies[0].get();
  }
  EXPECT_EQ("plaintext_value123XYZ", cookie_name->Value());
  EXPECT_EQ("something456ABC", cookie_other->Value());
  DestroyStore();
  cookies.clear();

  // Verify that "value" is now visible in the file.
  contents = ReadRawDBContents();
  EXPECT_NE(0U, contents.length());
  EXPECT_NE(contents.find("value123XYZ"), std::string::npos);
}

bool CompareCookies(const std::unique_ptr<CanonicalCookie>& a,
                    const std::unique_ptr<CanonicalCookie>& b) {
  return a->PartialCompare(*b);
}

bool CreateV9Schema(sql::Database* db) {
  sql::MetaTable meta_table;
  if (!meta_table.Init(db, 9 /* version */,
                       3 /* earliest compatible version */)) {
    return false;
  }

  // Version 9 schema
  std::string stmt(
      base::StringPrintf("CREATE TABLE cookies ("
                         "creation_utc INTEGER NOT NULL UNIQUE PRIMARY KEY,"
                         "host_key TEXT NOT NULL,"
                         "name TEXT NOT NULL,"
                         "value TEXT NOT NULL,"
                         "path TEXT NOT NULL,"
                         "expires_utc INTEGER NOT NULL,"
                         "secure INTEGER NOT NULL,"
                         "httponly INTEGER NOT NULL,"
                         "last_access_utc INTEGER NOT NULL, "
                         "has_expires INTEGER NOT NULL DEFAULT 1, "
                         "persistent INTEGER NOT NULL DEFAULT 1,"
                         "priority INTEGER NOT NULL DEFAULT 0,"
                         "encrypted_value BLOB DEFAULT '',"
                         "firstpartyonly INTEGER NOT NULL DEFAULT 0)"));
  if (!db->Execute(stmt.c_str()))
    return false;
  if (!db->Execute("CREATE INDEX domain ON cookies(host_key)"))
    return false;
  if (!db->Execute("CREATE INDEX is_transient ON cookies(persistent) "
                   "where persistent != 1")) {
    return false;
  }
  return true;
}

bool AddV9CookiesToDBImpl(sql::Database* db,
                          const std::vector<CanonicalCookie>& cookies);

// Add a selection of cookies to the DB.
bool AddV9CookiesToDB(sql::Database* db) {
  static base::Time cookie_time(base::Time::Now());

  std::vector<CanonicalCookie> cookies;
  cookies.push_back(CanonicalCookie(
      "A", "B", "example.com", "/", cookie_time, cookie_time, cookie_time,
      false, false, CookieSameSite::DEFAULT_MODE, COOKIE_PRIORITY_DEFAULT));
  cookie_time += base::TimeDelta::FromMicroseconds(1);
  cookies.push_back(CanonicalCookie(
      "C", "B", "example.com", "/", cookie_time, cookie_time, cookie_time,
      false, false, CookieSameSite::DEFAULT_MODE, COOKIE_PRIORITY_DEFAULT));
  cookie_time += base::TimeDelta::FromMicroseconds(1);
  cookies.push_back(CanonicalCookie(
      "A", "B", "example2.com", "/", cookie_time, cookie_time, cookie_time,
      false, false, CookieSameSite::DEFAULT_MODE, COOKIE_PRIORITY_DEFAULT));
  cookie_time += base::TimeDelta::FromMicroseconds(1);
  cookies.push_back(CanonicalCookie(
      "C", "B", "example2.com", "/", cookie_time, cookie_time, cookie_time,
      false, false, CookieSameSite::DEFAULT_MODE, COOKIE_PRIORITY_DEFAULT));
  cookie_time += base::TimeDelta::FromMicroseconds(1);
  cookies.push_back(CanonicalCookie(
      "A", "B", "example.com", "/path", cookie_time, cookie_time, cookie_time,
      false, false, CookieSameSite::DEFAULT_MODE, COOKIE_PRIORITY_DEFAULT));
  cookie_time += base::TimeDelta::FromMicroseconds(1);
  cookies.push_back(CanonicalCookie(
      "C", "B", "example.com", "/path", cookie_time, cookie_time, cookie_time,
      false, false, CookieSameSite::DEFAULT_MODE, COOKIE_PRIORITY_DEFAULT));
  cookie_time += base::TimeDelta::FromMicroseconds(1);
  return AddV9CookiesToDBImpl(db, cookies);
}

bool AddV9CookiesToDBImpl(sql::Database* db,
                          const std::vector<CanonicalCookie>& cookies) {
  sql::Statement add_smt(db->GetCachedStatement(
      SQL_FROM_HERE,
      "INSERT INTO cookies (creation_utc, host_key, name, value, "
      "encrypted_value, path, expires_utc, secure, httponly, firstpartyonly, "
      "last_access_utc, has_expires, persistent, priority) "
      "VALUES (?,?,?,?,?,?,?,?,?,?,?,?,?,?)"));
  if (!add_smt.is_valid())
    return false;
  sql::Transaction transaction(db);
  transaction.Begin();
  for (size_t i = 0; i < cookies.size(); ++i) {
    add_smt.Reset(true);
    add_smt.BindInt64(
        0,
        cookies[i].CreationDate().ToDeltaSinceWindowsEpoch().InMicroseconds());
    add_smt.BindString(1, cookies[i].Domain());
    add_smt.BindString(2, cookies[i].Name());
    add_smt.BindString(3, cookies[i].Value());
    add_smt.BindBlob(4, "", 0);  // encrypted_value
    add_smt.BindString(5, cookies[i].Path());
    add_smt.BindInt64(
        6, cookies[i].ExpiryDate().ToDeltaSinceWindowsEpoch().InMicroseconds());
    add_smt.BindInt(7, cookies[i].IsSecure());
    add_smt.BindInt(8, cookies[i].IsHttpOnly());
    // Note that this and Priority() below nominally rely on the enums in
    // sqlite_persistent_cookie_store.cc having the same values as the
    // ones in ../../cookies/cookie_constants.h.  But nothing in this test
    // relies on that equivalence, so it's not worth the hassle to guarantee
    // that.
    add_smt.BindInt(9, static_cast<int>(cookies[i].SameSite()));
    add_smt.BindInt64(10, cookies[i]
                              .LastAccessDate()
                              .ToDeltaSinceWindowsEpoch()
                              .InMicroseconds());
    add_smt.BindInt(11, cookies[i].IsPersistent());
    add_smt.BindInt(12, cookies[i].IsPersistent());
    add_smt.BindInt(13, static_cast<int>(cookies[i].Priority()));
    if (!add_smt.Run())
      return false;
  }
  if (!transaction.Commit())
    return false;

  return true;
}

// Confirm the cookie list passed in has the above cookies in it.
void ConfirmV9CookiesFromDB(
    std::vector<std::unique_ptr<CanonicalCookie>> read_in_cookies) {
  std::sort(read_in_cookies.begin(), read_in_cookies.end(), &CompareCookies);
  int i = 0;
  EXPECT_EQ("A", read_in_cookies[i]->Name());
  EXPECT_EQ("B", read_in_cookies[i]->Value());
  EXPECT_EQ("example.com", read_in_cookies[i]->Domain());
  EXPECT_EQ("/", read_in_cookies[i]->Path());

  i++;
  EXPECT_EQ("A", read_in_cookies[i]->Name());
  EXPECT_EQ("B", read_in_cookies[i]->Value());
  EXPECT_EQ("example.com", read_in_cookies[i]->Domain());
  EXPECT_EQ("/path", read_in_cookies[i]->Path());

  i++;
  EXPECT_EQ("A", read_in_cookies[i]->Name());
  EXPECT_EQ("B", read_in_cookies[i]->Value());
  EXPECT_EQ("example2.com", read_in_cookies[i]->Domain());
  EXPECT_EQ("/", read_in_cookies[i]->Path());

  i++;
  EXPECT_EQ("C", read_in_cookies[i]->Name());
  EXPECT_EQ("B", read_in_cookies[i]->Value());
  EXPECT_EQ("example.com", read_in_cookies[i]->Domain());
  EXPECT_EQ("/", read_in_cookies[i]->Path());

  i++;
  EXPECT_EQ("C", read_in_cookies[i]->Name());
  EXPECT_EQ("B", read_in_cookies[i]->Value());
  EXPECT_EQ("example.com", read_in_cookies[i]->Domain());
  EXPECT_EQ("/path", read_in_cookies[i]->Path());

  i++;
  EXPECT_EQ("C", read_in_cookies[i]->Name());
  EXPECT_EQ("B", read_in_cookies[i]->Value());
  EXPECT_EQ("example2.com", read_in_cookies[i]->Domain());
  EXPECT_EQ("/", read_in_cookies[i]->Path());
}

// Confirm that upgrading from a cookie store created with the
// creation time uniqueness constraint to one with the (name, domain, path)
// uniqueness constraint works with a good DB.
TEST_F(SQLitePersistentCookieStoreTest, UpgradeToSchemaVersion10) {
  // Open db
  sql::Database connection;
  ASSERT_TRUE(connection.Open(temp_dir_.GetPath().Append(kCookieFilename)));
  ASSERT_TRUE(CreateV9Schema(&connection));
  ASSERT_TRUE(AddV9CookiesToDB(&connection));
  connection.Close();

  std::vector<std::unique_ptr<CanonicalCookie>> read_in_cookies;
  CreateAndLoad(false, false, &read_in_cookies);
  ConfirmV9CookiesFromDB(std::move(read_in_cookies));
}

// Confirm that upgrading from a cookie store created with the
// creation time uniqueness constraint to one with the (name, domain, path)
// uniqueness constraint works with a corrupted DB.
TEST_F(SQLitePersistentCookieStoreTest, UpgradeToSchemaVersion10Corrupted) {
  // Open db
  sql::Database connection;
  ASSERT_TRUE(connection.Open(temp_dir_.GetPath().Append(kCookieFilename)));

  ASSERT_TRUE(CreateV9Schema(&connection));

  base::Time old_time = base::Time::Now() - base::TimeDelta::FromMinutes(90);
  base::Time old_time2 = base::Time::Now() - base::TimeDelta::FromMinutes(91);
  CanonicalCookie old_cookie1(
      "A", "old_value", "example.com", "/", old_time, old_time, old_time, false,
      false, CookieSameSite::DEFAULT_MODE, COOKIE_PRIORITY_DEFAULT);
  AddV9CookiesToDBImpl(&connection, {old_cookie1});

  // Add the same set of cookies twice to create duplicates.
  ASSERT_TRUE(AddV9CookiesToDB(&connection));
  ASSERT_TRUE(AddV9CookiesToDB(&connection));

  // Add some others as well.
  CanonicalCookie old_cookie2(
      "A", "old_value", "example.com", "/path", old_time2, old_time2, old_time2,
      false, false, CookieSameSite::DEFAULT_MODE, COOKIE_PRIORITY_DEFAULT);
  AddV9CookiesToDBImpl(&connection, {old_cookie2});

  connection.Close();

  std::vector<std::unique_ptr<CanonicalCookie>> read_in_cookies;
  CreateAndLoad(false, false, &read_in_cookies);
  // Finding failures of the uniqueness constraint should resolve them by
  // timestamp.
  ConfirmV9CookiesFromDB(std::move(read_in_cookies));
}

// Confirm the store can handle having cookies with identical creation
// times stored in it.
TEST_F(SQLitePersistentCookieStoreTest, IdenticalCreationTimes) {
  InitializeStore(false, false);
  base::Time cookie_time(base::Time::Now());
  base::Time cookie_expiry(cookie_time + base::TimeDelta::FromDays(1));
  AddCookieWithExpiration("A", "B", "example.com", "/", cookie_time,
                          cookie_expiry);
  AddCookieWithExpiration("C", "B", "example.com", "/", cookie_time,
                          cookie_expiry);
  AddCookieWithExpiration("A", "B", "example2.com", "/", cookie_time,
                          cookie_expiry);
  AddCookieWithExpiration("C", "B", "example2.com", "/", cookie_time,
                          cookie_expiry);
  AddCookieWithExpiration("A", "B", "example.com", "/path", cookie_time,
                          cookie_expiry);
  AddCookieWithExpiration("C", "B", "example.com", "/path", cookie_time,
                          cookie_expiry);
  Flush();
  DestroyStore();

  std::vector<std::unique_ptr<CanonicalCookie>> read_in_cookies;
  CreateAndLoad(false, false, &read_in_cookies);
  ASSERT_EQ(6u, read_in_cookies.size());

  std::sort(read_in_cookies.begin(), read_in_cookies.end(), &CompareCookies);
  int i = 0;
  EXPECT_EQ("A", read_in_cookies[i]->Name());
  EXPECT_EQ("example.com", read_in_cookies[i]->Domain());
  EXPECT_EQ("/", read_in_cookies[i]->Path());

  i++;
  EXPECT_EQ("A", read_in_cookies[i]->Name());
  EXPECT_EQ("example.com", read_in_cookies[i]->Domain());
  EXPECT_EQ("/path", read_in_cookies[i]->Path());

  i++;
  EXPECT_EQ("A", read_in_cookies[i]->Name());
  EXPECT_EQ("example2.com", read_in_cookies[i]->Domain());
  EXPECT_EQ("/", read_in_cookies[i]->Path());

  i++;
  EXPECT_EQ("C", read_in_cookies[i]->Name());
  EXPECT_EQ("example.com", read_in_cookies[i]->Domain());
  EXPECT_EQ("/", read_in_cookies[i]->Path());

  i++;
  EXPECT_EQ("C", read_in_cookies[i]->Name());
  EXPECT_EQ("example.com", read_in_cookies[i]->Domain());
  EXPECT_EQ("/path", read_in_cookies[i]->Path());

  i++;
  EXPECT_EQ("C", read_in_cookies[i]->Name());
  EXPECT_EQ("example2.com", read_in_cookies[i]->Domain());
  EXPECT_EQ("/", read_in_cookies[i]->Path());
}

TEST_F(SQLitePersistentCookieStoreTest, KeyInconsistency) {
  // Regression testcase for previous disagreement between CookieMonster
  // and SQLitePersistentCookieStoreTest as to what keys to LoadCookiesForKey
  // mean. The particular example doesn't, of course, represent an actual in-use
  // scenario, but while the inconstancy could happen with chrome-extension
  // URLs in real life, it was irrelevant for them in practice since their
  // rows would get key = "" which would get sorted before actual domains,
  // and therefore get loaded first by CookieMonster::FetchAllCookiesIfNecessary
  // with the task runners involved ensuring that would finish before the
  // incorrect LoadCookiesForKey got the chance to run.
  //
  // This test uses a URL that used to be treated differently by the two
  // layers that also sorts after other rows to avoid this scenario.

  // SQLitePersistentCookieStore will run its callbacks on what's passed to it
  // as |client_task_runner|, and CookieMonster expects to get callbacks from
  // its PersistentCookieStore on the same thread as its methods are invoked on;
  // so to avoid needing to post every CookieMonster API call, this uses the
  // current thread for SQLitePersistentCookieStore's |client_task_runner|.
  Create(false, false, true /* use_current_thread */);

  // Create a cookie on a scheme that doesn't handle cookies by default,
  // and save it.
  std::unique_ptr<CookieMonster> cookie_monster =
      std::make_unique<CookieMonster>(store_.get(), nullptr, nullptr);
  cookie_monster->SetCookieableSchemes({"gopher", "http"});
  ResultSavingCookieCallback<bool> set_cookie_callback;
  cookie_monster->SetCookieWithOptionsAsync(
      GURL("gopher://subdomain.gopheriffic.com/page"), "A=B; max-age=3600",
      CookieOptions(),
      base::BindOnce(&ResultSavingCookieCallback<bool>::Run,
                     base::Unretained(&set_cookie_callback)));
  set_cookie_callback.WaitUntilDone();
  EXPECT_TRUE(set_cookie_callback.result());

  // Also insert a whole bunch of cookies to slow down the background loading of
  // all the cookies.
  for (int i = 0; i < 50; ++i) {
    ResultSavingCookieCallback<bool> set_cookie_callback2;
    cookie_monster->SetCookieWithOptionsAsync(
        GURL(base::StringPrintf("http://example%d.com/", i)),
        "A=B; max-age=3600", CookieOptions(),
        base::BindOnce(&ResultSavingCookieCallback<bool>::Run,
                       base::Unretained(&set_cookie_callback2)));
    set_cookie_callback2.WaitUntilDone();
    EXPECT_TRUE(set_cookie_callback2.result());
  }

  net::TestClosure flush_closure;
  cookie_monster->FlushStore(flush_closure.closure());
  flush_closure.WaitForResult();
  cookie_monster = nullptr;

  // Re-create the PersistentCookieStore & CookieMonster. Note that the
  // destroyed store's ops will happen on same runners as the previous
  // instances, so they should complete before the new PersistentCookieStore
  // starts looking at the state on disk.
  Create(false, false, true /* want current thread to invoke cookie monster */);
  cookie_monster =
      std::make_unique<CookieMonster>(store_.get(), nullptr, nullptr);
  cookie_monster->SetCookieableSchemes({"gopher", "http"});

  // Now try to get the cookie back.
  GetCookieListCallback get_callback;
  cookie_monster->GetCookieListWithOptionsAsync(
      GURL("gopher://subdomain.gopheriffic.com/page"), CookieOptions(),
      base::BindOnce(&GetCookieListCallback::Run,
                     base::Unretained(&get_callback)));
  get_callback.WaitUntilDone();
  ASSERT_EQ(1u, get_callback.cookies().size());
  EXPECT_EQ("A", get_callback.cookies()[0].Name());
  EXPECT_EQ("B", get_callback.cookies()[0].Value());
  EXPECT_EQ("subdomain.gopheriffic.com", get_callback.cookies()[0].Domain());
};

TEST_F(SQLitePersistentCookieStoreTest, OpsIfInitFailed) {
  // Test to make sure we don't leak pending operations when initialization
  // fails really hard. To inject the failure, we put a directory where the
  // database file ought to be. This test relies on an external leak checker
  // (e.g. lsan) to actual catch thing.
  ASSERT_TRUE(
      base::CreateDirectory(temp_dir_.GetPath().Append(kCookieFilename)));
  Create(false, false, true /* want current thread to invoke cookie monster */);
  std::unique_ptr<CookieMonster> cookie_monster =
      std::make_unique<CookieMonster>(store_.get(), nullptr, nullptr);

  ResultSavingCookieCallback<bool> set_cookie_callback;
  cookie_monster->SetCookieWithOptionsAsync(
      GURL("http://www.example.com/"), "A=B; max-age=3600", CookieOptions(),
      base::BindOnce(&ResultSavingCookieCallback<bool>::Run,
                     base::Unretained(&set_cookie_callback)));
  set_cookie_callback.WaitUntilDone();
  EXPECT_TRUE(set_cookie_callback.result());

  // Things should commit once going out of scope.
}

TEST_F(SQLitePersistentCookieStoreTest, Coalescing) {
  enum class Op { kAdd, kDelete, kUpdate };

  struct TestCase {
    std::vector<Op> operations;
    size_t expected_queue_length;
  };

  std::vector<TestCase> testcases = {
      {{Op::kAdd, Op::kDelete}, 1u},
      {{Op::kUpdate, Op::kDelete}, 1u},
      {{Op::kAdd, Op::kUpdate, Op::kDelete}, 1u},
      {{Op::kUpdate, Op::kUpdate}, 1u},
      {{Op::kAdd, Op::kUpdate, Op::kUpdate}, 2u},
      {{Op::kDelete, Op::kAdd}, 2u},
      {{Op::kDelete, Op::kAdd, Op::kUpdate}, 3u},
      {{Op::kDelete, Op::kAdd, Op::kUpdate, Op::kUpdate}, 3u},
      {{Op::kDelete, Op::kDelete}, 1u},
      {{Op::kDelete, Op::kAdd, Op::kDelete}, 1u},
      {{Op::kDelete, Op::kAdd, Op::kUpdate, Op::kDelete}, 1u}};

  std::unique_ptr<CanonicalCookie> cookie =
      CanonicalCookie::Create(GURL("http://www.example.com/path"), "Tasty=Yes",
                              base::Time::Now(), CookieOptions());

  for (const TestCase& testcase : testcases) {
    Create(false, false, true /* want current thread to invoke the store. */);

    base::RunLoop run_loop;
    store_->Load(base::BindLambdaForTesting(
                     [&](CanonicalCookieVector cookies) { run_loop.Quit(); }),
                 NetLogWithSource());
    run_loop.Run();

    // Wedge the background thread to make sure that it doesn't start consuming
    // the queue.
    background_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&SQLitePersistentCookieStoreTest::WaitOnDBEvent,
                       base::Unretained(this)));

    // Now run the ops, and check how much gets queued.
    for (const Op op : testcase.operations) {
      switch (op) {
        case Op::kAdd:
          store_->AddCookie(*cookie);
          break;

        case Op::kDelete:
          store_->DeleteCookie(*cookie);
          break;

        case Op::kUpdate:
          store_->UpdateCookieAccessTime(*cookie);
          break;
      }
    }

    EXPECT_EQ(testcase.expected_queue_length,
              store_->GetQueueLengthForTesting());

    db_thread_event_.Signal();
  }
}

TEST_F(SQLitePersistentCookieStoreTest, NoCoalesceUnrelated) {
  Create(false, false, true /* want current thread to invoke the store. */);

  base::RunLoop run_loop;
  store_->Load(base::BindLambdaForTesting(
                   [&](CanonicalCookieVector cookies) { run_loop.Quit(); }),
               NetLogWithSource());
  run_loop.Run();

  std::unique_ptr<CanonicalCookie> cookie1 =
      CanonicalCookie::Create(GURL("http://www.example.com/path"), "Tasty=Yes",
                              base::Time::Now(), CookieOptions());

  std::unique_ptr<CanonicalCookie> cookie2 =
      CanonicalCookie::Create(GURL("http://not.example.com/path"), "Tasty=No",
                              base::Time::Now(), CookieOptions());

  // Wedge the background thread to make sure that it doesn't start consuming
  // the queue.
  background_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&SQLitePersistentCookieStoreTest::WaitOnDBEvent,
                                base::Unretained(this)));

  store_->AddCookie(*cookie1);
  store_->AddCookie(*cookie2);
  // delete on cookie2 shouldn't cancel op on unrelated cookie1.
  EXPECT_EQ(2u, store_->GetQueueLengthForTesting());

  db_thread_event_.Signal();
}

}  // namespace net
