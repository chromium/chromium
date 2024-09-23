// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/extras/sqlite/sqlite_persistent_cookie_store.h"

#include <stdint.h>

#include <map>
#include <memory>
#include <optional>
#include <set>
#include <utility>
#include <vector>

#include "base/containers/span.h"
#include "base/files/file.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/location.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/run_loop.h"
#include "base/sequence_checker.h"
#include "base/strings/stringprintf.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/threading/thread_restrictions.h"
#include "base/time/time.h"
#include "crypto/encryptor.h"
#include "crypto/symmetric_key.h"
#include "net/base/features.h"
#include "net/base/test_completion_callback.h"
#include "net/cookies/canonical_cookie.h"
#include "net/cookies/cookie_constants.h"
#include "net/cookies/cookie_inclusion_status.h"
#include "net/cookies/cookie_store_test_callbacks.h"
#include "net/extras/sqlite/cookie_crypto_delegate.h"
#include "net/log/net_log_capture_mode.h"
#include "net/log/test_net_log.h"
#include "net/log/test_net_log_util.h"
#include "net/test/test_with_task_environment.h"
#include "sql/database.h"
#include "sql/meta_table.h"
#include "sql/statement.h"
#include "sql/transaction.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/third_party/mozilla/url_parse.h"

namespace net {

namespace {

const base::FilePath::CharType kCookieFilename[] = FILE_PATH_LITERAL("Cookies");

class CookieCryptor : public CookieCryptoDelegate {
 public:
  CookieCryptor();

  // net::CookieCryptoDelegate implementation.
  void Init(base::OnceClosure callback) override;
  bool EncryptString(const std::string& plaintext,
                     std::string* ciphertext) override;
  bool DecryptString(const std::string& ciphertext,
                     std::string* plaintext) override;

  // Obtain a closure that can be called to trigger an initialize. If this
  // instance has already been destructed then the returned base::OnceClosure
  // does nothing. This allows tests to pass ownership to the CookieCryptor
  // while still retaining a weak reference to the Init function.
  base::OnceClosure GetInitClosure(base::OnceClosure callback);

 private:
  void InitComplete();
  bool init_ GUARDED_BY_CONTEXT(sequence_checker_) = false;
  bool initing_ GUARDED_BY_CONTEXT(sequence_checker_) = false;
  base::OnceClosureList callbacks_ GUARDED_BY_CONTEXT(sequence_checker_);
  std::unique_ptr<crypto::SymmetricKey> key_;
  crypto::Encryptor encryptor_;
  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<CookieCryptor> weak_ptr_factory_{this};
};

CookieCryptor::CookieCryptor()
    : key_(crypto::SymmetricKey::DeriveKeyFromPasswordUsingPbkdf2(
          crypto::SymmetricKey::AES,
          "password",
          "saltiest",
          1000,
          256)) {
  std::string iv("the iv: 16 bytes");
  encryptor_.Init(key_.get(), crypto::Encryptor::CBC, iv);
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

base::OnceClosure CookieCryptor::GetInitClosure(base::OnceClosure callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return base::BindOnce(&CookieCryptor::Init, weak_ptr_factory_.GetWeakPtr(),
                        std::move(callback));
}

void CookieCryptor::Init(base::OnceClosure callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (init_) {
    std::move(callback).Run();
    return;
  }

  // Callbacks here are owned by test fixtures that outlive the CookieCryptor.
  callbacks_.AddUnsafe(std::move(callback));

  if (initing_) {
    return;
  }

  initing_ = true;
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&CookieCryptor::InitComplete,
                     weak_ptr_factory_.GetWeakPtr()),
      base::Milliseconds(100));
}

bool CookieCryptor::EncryptString(const std::string& plaintext,
                                  std::string* ciphertext) {
  // SQLite crypto uses OSCrypt Async Encryptor and the behavior for empty
  // plaintext is to return empty ciphertext. See
  // os_crypt_async::Encryptor::EncryptString. This matches this behavior,
  // without adding a dependency from net into components.
  if (plaintext.empty()) {
    ciphertext->clear();
    return true;
  }
  return encryptor_.Encrypt(plaintext, ciphertext);
}

bool CookieCryptor::DecryptString(const std::string& ciphertext,
                                  std::string* plaintext) {
  // SQLite crypto uses OSCrypt Async Encryptor and the behavior for empty
  // ciphertext is to return empty plaintext. See
  // os_crypt_async::Encryptor::DecryptString. This matches this behavior,
  // without adding a dependency from net into components.
  if (ciphertext.empty()) {
    plaintext->clear();
    return true;
  }
  return encryptor_.Decrypt(ciphertext, plaintext);
}

void CookieCryptor::InitComplete() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  init_ = true;
  callbacks_.Notify();
}

// Matches the CanonicalCookie's strictly_unique_key and last_access_date
// against a unique_ptr<CanonicalCookie>.
MATCHER_P2(MatchesCookieKeyAndLastAccessDate,
           StrictlyUniqueKey,
           last_access_date,
           "") {
  if (!arg) {
    return false;
  }
  const CanonicalCookie& list_cookie = *arg;

  return testing::ExplainMatchResult(StrictlyUniqueKey,
                                     list_cookie.StrictlyUniqueKey(),
                                     result_listener) &&
         testing::ExplainMatchResult(
             last_access_date, list_cookie.LastAccessDate(), result_listener);
}

// Matches every field of a CanonicalCookie against a
// unique_ptr<CanonicalCookie>.
MATCHER_P(MatchesEveryCookieField, cookie, "") {
  if (!arg) {
    return false;
  }
  const CanonicalCookie& list_cookie = *arg;
  return cookie.HasEquivalentDataMembers(list_cookie);
}

}  // namespace

typedef std::vector<std::unique_ptr<CanonicalCookie>> CanonicalCookieVector;

class SQLitePersistentCookieStoreTest : public TestWithTaskEnvironment {
 public:
  SQLitePersistentCookieStoreTest()
      : loaded_event_(base::WaitableEvent::ResetPolicy::AUTOMATIC,
                      base::WaitableEvent::InitialState::NOT_SIGNALED),
        db_thread_event_(base::WaitableEvent::ResetPolicy::AUTOMATIC,
                         base::WaitableEvent::InitialState::NOT_SIGNALED) {}

  void SignalLoadedEvent() { loaded_event_.Signal(); }

  void OnLoaded(base::OnceClosure closure, CanonicalCookieVector cookies) {
    cookies_.swap(cookies);
    std::move(closure).Run();
  }

  CanonicalCookieVector Load() {
    base::RunLoop run_loop;
    CanonicalCookieVector cookies;
    store_->Load(
        base::BindLambdaForTesting([&](CanonicalCookieVector obtained_cookies) {
          cookies.swap(obtained_cookies);
          run_loop.Quit();
        }),
        NetLogWithSource::Make(NetLogSourceType::NONE));
    run_loop.Run();
    return cookies;
  }

  void LoadAsyncAndSignalEvent() {
    store_->Load(
        base::BindOnce(
            &SQLitePersistentCookieStoreTest::OnLoaded, base::Unretained(this),
            base::BindOnce(&SQLitePersistentCookieStoreTest::SignalLoadedEvent,
                           base::Unretained(this))),
        NetLogWithSource::Make(NetLogSourceType::NONE));
  }

  void Flush() {
    base::RunLoop run_loop;
    store_->Flush(run_loop.QuitClosure());
    run_loop.Run();
  }

  void DestroyStore() {
    store_ = nullptr;
    // Make sure we wait until the destructor has run by running all
    // TaskEnvironment tasks.
    RunUntilIdle();
  }

  void Create(bool crypt_cookies,
              bool restore_old_session_cookies,
              bool use_current_thread,
              bool enable_exclusive_access) {
    store_ = base::MakeRefCounted<SQLitePersistentCookieStore>(
        temp_dir_.GetPath().Append(kCookieFilename),
        use_current_thread ? base::SingleThreadTaskRunner::GetCurrentDefault()
                           : client_task_runner_,
        background_task_runner_, restore_old_session_cookies,
        crypt_cookies ? std::make_unique<CookieCryptor>() : nullptr,
        enable_exclusive_access);
  }

  CanonicalCookieVector CreateAndLoad(bool crypt_cookies,
                                      bool restore_old_session_cookies) {
    Create(crypt_cookies, restore_old_session_cookies,
           /*use_current_thread=*/false, /*enable_exclusive_access=*/false);
    return Load();
  }

  void InitializeStore(bool crypt, bool restore_old_session_cookies) {
    EXPECT_EQ(0U, CreateAndLoad(crypt, restore_old_session_cookies).size());
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
    store_->AddCookie(*CanonicalCookie::CreateUnsafeCookieForTesting(
        name, value, domain, path, creation, /*expiration=*/creation,
        /*last_access=*/base::Time(), /*last_update=*/base::Time(),
        /*secure=*/false,
        /*httponly=*/false, CookieSameSite::NO_RESTRICTION,
        COOKIE_PRIORITY_DEFAULT));
  }

  void AddCookieWithExpiration(const std::string& name,
                               const std::string& value,
                               const std::string& domain,
                               const std::string& path,
                               const base::Time& creation,
                               const base::Time& expiration) {
    store_->AddCookie(*CanonicalCookie::CreateUnsafeCookieForTesting(
        name, value, domain, path, creation, expiration,
        /*last_access=*/base::Time(), /*last_update=*/base::Time(),
        /*secure=*/false,
        /*httponly=*/false, CookieSameSite::NO_RESTRICTION,
        COOKIE_PRIORITY_DEFAULT));
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
    if (!expect_init_errors_) {
      EXPECT_THAT(histograms_.GetAllSamples("Cookie.ErrorInitializeDB"),
                  ::testing::IsEmpty());
    }
    DestroyStore();
  }

 protected:
  const scoped_refptr<base::SequencedTaskRunner> background_task_runner_ =
      base::ThreadPool::CreateSequencedTaskRunner({base::MayBlock()});
  const scoped_refptr<base::SequencedTaskRunner> client_task_runner_ =
      base::ThreadPool::CreateSequencedTaskRunner({base::MayBlock()});
  base::WaitableEvent loaded_event_;
  base::WaitableEvent db_thread_event_;
  CanonicalCookieVector cookies_;
  base::ScopedTempDir temp_dir_;
  scoped_refptr<SQLitePersistentCookieStore> store_;
  std::unique_ptr<CookieCryptor> cookie_crypto_delegate_;
  base::HistogramTester histograms_;
  bool expect_init_errors_ = false;
};

TEST_F(SQLitePersistentCookieStoreTest, TestInvalidVersionRecovery) {
  InitializeStore(/*crypt=*/false, /*restore_old_session_cookies=*/false);
  AddCookie("A", "B", "foo.bar", "/", base::Time::Now());
  DestroyStore();

  // Load up the store and verify that it has good data in it.
  CanonicalCookieVector cookies = CreateAndLoad(
      /*crypt_cookies=*/false, /*restore_old_session_cookies=*/false);
  ASSERT_EQ(1U, cookies.size());
  ASSERT_STREQ("foo.bar", cookies[0]->Domain().c_str());
  ASSERT_STREQ("A", cookies[0]->Name().c_str());
  ASSERT_STREQ("B", cookies[0]->Value().c_str());
  DestroyStore();
  cookies.clear();

  // Now make the version too old to initialize from.
  {
    sql::Database db;
    ASSERT_TRUE(db.Open(temp_dir_.GetPath().Append(kCookieFilename)));
    sql::MetaTable meta_table;
    ASSERT_TRUE(meta_table.Init(&db, 1, 1));
    // Keep in sync with latest unsupported version from:
    // net/extras/sqlite/sqlite_persistent_cookie_store.cc
    ASSERT_TRUE(meta_table.SetVersionNumber(17));
  }

  // Upon loading, the database should be reset to a good, blank state.
  cookies = CreateAndLoad(/*crypt_cookies=*/false,
                          /*restore_old_session_cookies=*/false);
  ASSERT_EQ(0U, cookies.size());

  // Verify that, after, recovery, the database persists properly.
  AddCookie("X", "Y", "foo.bar", "/", base::Time::Now());
  DestroyStore();
  cookies = CreateAndLoad(/*crypt_cookies=*/false,
                          /*restore_old_session_cookies=*/false);
  ASSERT_EQ(1U, cookies.size());
  ASSERT_STREQ("foo.bar", cookies[0]->Domain().c_str());
  ASSERT_STREQ("X", cookies[0]->Name().c_str());
  ASSERT_STREQ("Y", cookies[0]->Value().c_str());
}

TEST_F(SQLitePersistentCookieStoreTest, TestInvalidMetaTableRecovery) {
  InitializeStore(/*crypt=*/false, /*restore_old_session_cookies=*/false);
  AddCookie("A", "B", "foo.bar", "/", base::Time::Now());
  DestroyStore();

  // Load up the store and verify that it has good data in it.
  CanonicalCookieVector cookies = CreateAndLoad(
      /*crypt_cookies=*/false, /*restore_old_session_cookies=*/false);
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
    sql::MetaTable meta_table;
    ASSERT_TRUE(meta_table.Init(&db, 1, 1));
    ASSERT_TRUE(db.Execute("DELETE FROM meta"));
  }

  // Upon loading, the database should be reset to a good, blank state.
  cookies = CreateAndLoad(/*crypt_cookies=*/false,
                          /*restore_old_session_cookies=*/false);
  ASSERT_EQ(0U, cookies.size());

  // Verify that, after, recovery, the database persists properly.
  AddCookie("X", "Y", "foo.bar", "/", base::Time::Now());
  DestroyStore();
  cookies = CreateAndLoad(/*crypt_cookies=*/false,
                          /*restore_old_session_cookies=*/false);
  ASSERT_EQ(1U, cookies.size());
  ASSERT_STREQ("foo.bar", cookies[0]->Domain().c_str());
  ASSERT_STREQ("X", cookies[0]->Name().c_str());
  ASSERT_STREQ("Y", cookies[0]->Value().c_str());
}

// Test if data is stored as expected in the SQLite database.
TEST_F(SQLitePersistentCookieStoreTest, TestPersistance) {
  InitializeStore(/*crypt=*/false, /*restore_old_session_cookies=*/false);
  AddCookie("A", "B", "foo.bar", "/", base::Time::Now());
  // Replace the store effectively destroying the current one and forcing it
  // to write its data to disk. Then we can see if after loading it again it
  // is still there.
  DestroyStore();
  // Reload and test for persistence
  CanonicalCookieVector cookies = CreateAndLoad(
      /*crypt_cookies=*/false, /*restore_old_session_cookies=*/false);
  ASSERT_EQ(1U, cookies.size());
  ASSERT_STREQ("foo.bar", cookies[0]->Domain().c_str());
  ASSERT_STREQ("A", cookies[0]->Name().c_str());
  ASSERT_STREQ("B", cookies[0]->Value().c_str());

  // Now delete the cookie and check persistence again.
  store_->DeleteCookie(*cookies[0]);
  DestroyStore();
  cookies.clear();

  // Reload and check if the cookie has been removed.
  cookies = CreateAndLoad(/*crypt_cookies=*/false,
                          /*restore_old_session_cookies=*/false);
  ASSERT_EQ(0U, cookies.size());
}

TEST_F(SQLitePersistentCookieStoreTest, TestSessionCookiesDeletedOnStartup) {
  // Initialize the cookie store with 3 persistent cookies, 5 transient
  // cookies.
  InitializeStore(/*crypt=*/false, /*restore_old_session_cookies=*/false);

  // Add persistent cookies.
  base::Time t = base::Time::Now();
  AddCookie("A", "B", "a1.com", "/", t);
  t += base::Microseconds(10);
  AddCookie("A", "B", "a2.com", "/", t);
  t += base::Microseconds(10);
  AddCookie("A", "B", "a3.com", "/", t);

  // Add transient cookies.
  t += base::Microseconds(10);
  AddCookieWithExpiration("A", "B", "b1.com", "/", t, base::Time());
  t += base::Microseconds(10);
  AddCookieWithExpiration("A", "B", "b2.com", "/", t, base::Time());
  t += base::Microseconds(10);
  AddCookieWithExpiration("A", "B", "b3.com", "/", t, base::Time());
  t += base::Microseconds(10);
  AddCookieWithExpiration("A", "B", "b4.com", "/", t, base::Time());
  t += base::Microseconds(10);
  AddCookieWithExpiration("A", "B", "b5.com", "/", t, base::Time());
  DestroyStore();

  // Load the store a second time. Before the store finishes loading, add a
  // transient cookie and flush it to disk.
  store_ = base::MakeRefCounted<SQLitePersistentCookieStore>(
      temp_dir_.GetPath().Append(kCookieFilename), client_task_runner_,
      background_task_runner_, false, nullptr, false);

  // Posting a blocking task to db_thread_ makes sure that the DB thread waits
  // until both Load and Flush have been posted to its task queue.
  background_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&SQLitePersistentCookieStoreTest::WaitOnDBEvent,
                                base::Unretained(this)));
  LoadAsyncAndSignalEvent();
  t += base::Microseconds(10);
  AddCookieWithExpiration("A", "B", "c.com", "/", t, base::Time());
  base::WaitableEvent event(base::WaitableEvent::ResetPolicy::AUTOMATIC,
                            base::WaitableEvent::InitialState::NOT_SIGNALED);
  store_->Flush(
      base::BindOnce(&base::WaitableEvent::Signal, base::Unretained(&event)));

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
  store_ = base::MakeRefCounted<SQLitePersistentCookieStore>(
      temp_dir_.GetPath().Append(kCookieFilename), client_task_runner_,
      background_task_runner_, true, nullptr, false);
  LoadAsyncAndSignalEvent();
  loaded_event_.Wait();
  ASSERT_EQ(4u, cookies_.size());
}

// Test that priority load of cookies for a specific domain key could be
// completed before the entire store is loaded.
TEST_F(SQLitePersistentCookieStoreTest, TestLoadCookiesForKey) {
  InitializeStore(/*crypt=*/true, /*restore_old_session_cookies=*/false);
  base::Time t = base::Time::Now();
  AddCookie("A", "B", "foo.bar", "/", t);
  t += base::Microseconds(10);
  AddCookie("A", "B", "www.aaa.com", "/", t);
  t += base::Microseconds(10);
  AddCookie("A", "B", "travel.aaa.com", "/", t);
  t += base::Microseconds(10);
  AddCookie("A", "B", "www.bbb.com", "/", t);
  DestroyStore();

  auto cookie_crypto_delegate = std::make_unique<CookieCryptor>();
  base::RunLoop cookie_crypto_loop;
  auto init_closure =
      cookie_crypto_delegate->GetInitClosure(cookie_crypto_loop.QuitClosure());

  // base::test::TaskEnvironment runs |background_task_runner_| and
  // |client_task_runner_| on the same thread. Therefore, when a
  // |background_task_runner_| task is blocked, |client_task_runner_| tasks
  // can't run. To allow precise control of |background_task_runner_| without
  // preventing client tasks to run, use
  // base::SingleThreadTaskRunner::GetCurrentDefault() instead of
  // |client_task_runner_| for this test.
  store_ = base::MakeRefCounted<SQLitePersistentCookieStore>(
      temp_dir_.GetPath().Append(kCookieFilename),
      base::SingleThreadTaskRunner::GetCurrentDefault(),
      background_task_runner_,
      /*restore_old_session_cookies=*/false, std::move(cookie_crypto_delegate),
      /*enable_exclusive_access=*/false);

  // Posting a blocking task to db_thread_ makes sure that the DB thread waits
  // until both Load and LoadCookiesForKey have been posted to its task queue.
  background_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&SQLitePersistentCookieStoreTest::WaitOnDBEvent,
                                base::Unretained(this)));
  RecordingNetLogObserver net_log_observer;
  LoadAsyncAndSignalEvent();
  base::RunLoop run_loop;
  net_log_observer.SetObserverCaptureMode(NetLogCaptureMode::kDefault);
  store_->LoadCookiesForKey(
      "aaa.com",
      base::BindOnce(&SQLitePersistentCookieStoreTest::OnLoaded,
                     base::Unretained(this), run_loop.QuitClosure()));

  // Complete the initialization of the cookie crypto delegate. This ensures
  // that any background tasks from the Load or the LoadCookiesForKey are posted
  // to the background_task_runner_.
  std::move(init_closure).Run();
  cookie_crypto_loop.Run();

  // Post a final blocking task to the background_task_runner_ to ensure no
  // other cookie loads take place during the test.
  background_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&SQLitePersistentCookieStoreTest::WaitOnDBEvent,
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
  auto entries = net_log_observer.GetEntries();
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
  EXPECT_FALSE(GetOptionalStringValueFromParams(entries[pos], "key"));
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

TEST_F(SQLitePersistentCookieStoreTest, TestBeforeCommitCallback) {
  InitializeStore(/*crypt=*/false, /*restore_old_session_cookies=*/false);

  struct Counter {
    int count = 0;
    void increment() { count++; }
  };

  Counter counter;
  store_->SetBeforeCommitCallback(
      base::BindRepeating(&Counter::increment, base::Unretained(&counter)));

  // The implementation of SQLitePersistentCookieStore::Backend flushes changes
  // after 30s or 512 pending operations. Add 512 cookies to the store to test
  // that the callback gets called when SQLitePersistentCookieStore internally
  // flushes its store.
  for (int i = 0; i < 512; i++) {
    // Each cookie needs a unique timestamp for creation_utc (see DB schema).
    base::Time t = base::Time::Now() + base::Microseconds(i);
    AddCookie(base::StringPrintf("%d", i), "foo", "example.com", "/", t);
  }

  RunUntilIdle();
  EXPECT_GT(counter.count, 0);

  DestroyStore();
}

// Test that we can force the database to be written by calling Flush().
TEST_F(SQLitePersistentCookieStoreTest, TestFlush) {
  InitializeStore(/*crypt=*/false, /*restore_old_session_cookies=*/false);
  // File timestamps don't work well on all platforms, so we'll determine
  // whether the DB file has been modified by checking its size.
  base::FilePath path = temp_dir_.GetPath().Append(kCookieFilename);
  base::File::Info info;
  ASSERT_TRUE(base::GetFileInfo(path, &info));
  int64_t base_size = info.size;

  // Write some large cookies, so the DB will have to expand by several KB.
  for (char c = 'a'; c < 'z'; ++c) {
    // Each cookie needs a unique timestamp for creation_utc (see DB schema).
    base::Time t = base::Time::Now() + base::Microseconds(c);
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
  InitializeStore(/*crypt=*/false, /*restore_old_session_cookies=*/true);

  // Add a session cookie.
  store_->AddCookie(*CanonicalCookie::CreateUnsafeCookieForTesting(
      "C", "D", "sessioncookie.com", "/", /*creation=*/base::Time::Now(),
      /*expiration=*/base::Time(),
      /*last_access=*/base::Time(), /*last_update=*/base::Time(),
      /*secure=*/false, /*httponly=*/false, CookieSameSite::NO_RESTRICTION,
      COOKIE_PRIORITY_DEFAULT));

  // Force the store to write its data to the disk.
  DestroyStore();

  // Create a store that loads session cookies and test that the session cookie
  // was loaded.
  CanonicalCookieVector cookies = CreateAndLoad(
      /*crypt_cookies=*/false, /*restore_old_session_cookies=*/true);

  ASSERT_EQ(1U, cookies.size());
  ASSERT_STREQ("sessioncookie.com", cookies[0]->Domain().c_str());
  ASSERT_STREQ("C", cookies[0]->Name().c_str());
  ASSERT_STREQ("D", cookies[0]->Value().c_str());
  ASSERT_EQ(COOKIE_PRIORITY_DEFAULT, cookies[0]->Priority());
}

// Test refusing to load old session cookies from the disk.
TEST_F(SQLitePersistentCookieStoreTest, TestDontLoadOldSessionCookies) {
  InitializeStore(/*crypt=*/false, /*restore_old_session_cookies=*/true);

  // Add a session cookie.
  store_->AddCookie(*CanonicalCookie::CreateUnsafeCookieForTesting(
      "C", "D", "sessioncookie.com", "/", /*creation=*/base::Time::Now(),
      /*expiration=*/base::Time(),
      /*last_access=*/base::Time(), /*last_update=*/base::Time(),
      /*secure=*/false, /*httponly=*/false, CookieSameSite::NO_RESTRICTION,
      COOKIE_PRIORITY_DEFAULT));

  // Force the store to write its data to the disk.
  DestroyStore();

  // Create a store that doesn't load old session cookies and test that the
  // session cookie was not loaded.
  CanonicalCookieVector cookies = CreateAndLoad(
      /*crypt_cookies=*/false, /*restore_old_session_cookies=*/false);
  ASSERT_EQ(0U, cookies.size());

  // The store should also delete the session cookie. Wait until that has been
  // done.
  DestroyStore();

  // Create a store that loads old session cookies and test that the session
  // cookie is gone.
  cookies = CreateAndLoad(/*crypt_cookies=*/false,
                          /*restore_old_session_cookies=*/true);
  ASSERT_EQ(0U, cookies.size());
}

// Confirm bad cookies on disk don't get looaded, and that we also remove them
// from the database.
TEST_F(SQLitePersistentCookieStoreTest, FilterBadCookiesAndFixupDb) {
  // Create an on-disk store.
  InitializeStore(/*crypt=*/false, /*restore_old_session_cookies=*/true);
  DestroyStore();

  // Add some cookies in by hand.
  base::FilePath store_name(temp_dir_.GetPath().Append(kCookieFilename));
  std::unique_ptr<sql::Database> db(std::make_unique<sql::Database>());
  ASSERT_TRUE(db->Open(store_name));
  sql::Statement stmt(db->GetUniqueStatement(
      "INSERT INTO cookies (creation_utc, host_key, top_frame_site_key, name, "
      "value, encrypted_value, path, expires_utc, is_secure, is_httponly, "
      "samesite, last_access_utc, has_expires, is_persistent, priority, "
      "source_scheme, source_port, last_update_utc, source_type, "
      "has_cross_site_ancestor) "
      "VALUES (?,?,?,?,?,'',?,0,0,0,0,0,1,1,0,?,?,?,0,0)"));
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
  base::Time last_update(base::Time::Now());
  for (auto& cookie_info : cookies_info) {
    stmt.Reset(true);

    stmt.BindInt64(0, creation_time++);
    stmt.BindString(1, cookie_info.domain);
    // TODO(crbug.com/40188414) Test some non-empty values when CanonicalCookie
    // supports partition key.
    stmt.BindString(2, net::kEmptyCookiePartitionKey);
    stmt.BindString(3, cookie_info.name);
    stmt.BindString(4, cookie_info.value);
    stmt.BindString(5, cookie_info.path);
    stmt.BindInt(6, static_cast<int>(CookieSourceScheme::kUnset));
    stmt.BindInt(7, SQLitePersistentCookieStore::kDefaultUnknownPort);
    stmt.BindTime(8, last_update);
    ASSERT_TRUE(stmt.Run());
  }
  stmt.Clear();
  db.reset();

  // Reopen the store and confirm that the only cookie loaded is the
  // canonical one on an unrelated domain.
  CanonicalCookieVector cookies = CreateAndLoad(
      /*crypt_cookies=*/false, /*restore_old_session_cookies=*/false);
  ASSERT_EQ(1U, cookies.size());
  EXPECT_STREQ("chromium.org", cookies[0]->Domain().c_str());
  EXPECT_STREQ("G", cookies[0]->Name().c_str());
  EXPECT_STREQ("H", cookies[0]->Value().c_str());
  EXPECT_STREQ("/dir", cookies[0]->Path().c_str());
  EXPECT_EQ(last_update, cookies[0]->LastUpdateDate());
  DestroyStore();

  // Make sure that we only have one row left.
  db = std::make_unique<sql::Database>();
  ASSERT_TRUE(db->Open(store_name));
  sql::Statement verify_stmt(db->GetUniqueStatement("SELECT * FROM COOKIES"));
  ASSERT_TRUE(verify_stmt.is_valid());

  EXPECT_TRUE(verify_stmt.Step());
  EXPECT_TRUE(verify_stmt.Succeeded());
  // Confirm only one match.
  EXPECT_FALSE(verify_stmt.Step());
}

TEST_F(SQLitePersistentCookieStoreTest, PersistIsPersistent) {
  InitializeStore(/*crypt=*/false, /*restore_old_session_cookies=*/true);
  static const char kSessionName[] = "session";
  static const char kPersistentName[] = "persistent";

  // Add a session cookie.
  store_->AddCookie(*CanonicalCookie::CreateUnsafeCookieForTesting(
      kSessionName, "val", "sessioncookie.com", "/",
      /*creation=*/base::Time::Now(),
      /*expiration=*/base::Time(), /*last_access=*/base::Time(),
      /*last_update=*/base::Time(), /*secure=*/false, /*httponly=*/false,
      CookieSameSite::NO_RESTRICTION, COOKIE_PRIORITY_DEFAULT));
  // Add a persistent cookie.
  store_->AddCookie(*CanonicalCookie::CreateUnsafeCookieForTesting(
      kPersistentName, "val", "sessioncookie.com", "/",
      /*creation=*/base::Time::Now() - base::Days(1),
      /*expiration=*/base::Time::Now() + base::Days(1),
      /*last_access=*/base::Time(), /*last_update=*/base::Time(),
      /*secure=*/false, /*httponly=*/false, CookieSameSite::NO_RESTRICTION,
      COOKIE_PRIORITY_DEFAULT));

  // Force the store to write its data to the disk.
  DestroyStore();

  // Create a store that loads session cookie and test that the IsPersistent
  // attribute is restored.
  CanonicalCookieVector cookies = CreateAndLoad(
      /*crypt_cookies=*/false, /*restore_old_session_cookies=*/true);
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
}

TEST_F(SQLitePersistentCookieStoreTest, PriorityIsPersistent) {
  static const char kDomain[] = "sessioncookie.com";
  static const char kLowName[] = "low";
  static const char kMediumName[] = "medium";
  static const char kHighName[] = "high";
  static const char kCookieValue[] = "value";
  static const char kCookiePath[] = "/";

  InitializeStore(/*crypt=*/false, /*restore_old_session_cookies=*/true);

  // Add a low-priority persistent cookie.
  store_->AddCookie(*CanonicalCookie::CreateUnsafeCookieForTesting(
      kLowName, kCookieValue, kDomain, kCookiePath,
      /*creation=*/base::Time::Now() - base::Minutes(1),
      /*expiration=*/base::Time::Now() + base::Days(1),
      /*last_access=*/base::Time(), /*last_update=*/base::Time(),
      /*secure=*/false, /*httponly=*/false, CookieSameSite::NO_RESTRICTION,
      COOKIE_PRIORITY_LOW));

  // Add a medium-priority persistent cookie.
  store_->AddCookie(*CanonicalCookie::CreateUnsafeCookieForTesting(
      kMediumName, kCookieValue, kDomain, kCookiePath,
      /*creation=*/base::Time::Now() - base::Minutes(2),
      /*expiration=*/base::Time::Now() + base::Days(1),
      /*last_access=*/base::Time(), /*last_update=*/base::Time(),
      /*secure=*/false, /*httponly=*/false, CookieSameSite::NO_RESTRICTION,
      COOKIE_PRIORITY_MEDIUM));

  // Add a high-priority persistent cookie.
  store_->AddCookie(*CanonicalCookie::CreateUnsafeCookieForTesting(
      kHighName, kCookieValue, kDomain, kCookiePath,
      /*creation=*/base::Time::Now() - base::Minutes(3),
      /*expiration=*/base::Time::Now() + base::Days(1),
      /*last_access=*/base::Time(), /*last_update=*/base::Time(),
      /*secure=*/false, /*httponly=*/false, CookieSameSite::NO_RESTRICTION,
      COOKIE_PRIORITY_HIGH));

  // Force the store to write its data to the disk.
  DestroyStore();

  // Create a store that loads session cookie and test that the priority
  // attribute values are restored.
  CanonicalCookieVector cookies = CreateAndLoad(
      /*crypt_cookies=*/false, /*restore_old_session_cookies=*/true);
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
}

TEST_F(SQLitePersistentCookieStoreTest, SameSiteIsPersistent) {
  const char kDomain[] = "sessioncookie.com";
  const char kNoneName[] = "none";
  const char kLaxName[] = "lax";
  const char kStrictName[] = "strict";
  const char kCookieValue[] = "value";
  const char kCookiePath[] = "/";

  InitializeStore(/*crypt=*/false, /*restore_old_session_cookies=*/true);

  // Add a non-samesite persistent cookie.
  store_->AddCookie(*CanonicalCookie::CreateUnsafeCookieForTesting(
      kNoneName, kCookieValue, kDomain, kCookiePath,
      /*creation=*/base::Time::Now() - base::Minutes(1),
      /*expiration=*/base::Time::Now() + base::Days(1),
      /*last_access=*/base::Time(), /*last_update=*/base::Time(),
      /*secure=*/false, /*httponly=*/false, CookieSameSite::NO_RESTRICTION,
      COOKIE_PRIORITY_DEFAULT));

  // Add a lax-samesite persistent cookie.
  store_->AddCookie(*CanonicalCookie::CreateUnsafeCookieForTesting(
      kLaxName, kCookieValue, kDomain, kCookiePath,
      /*creation=*/base::Time::Now() - base::Minutes(2),
      /*expiration=*/base::Time::Now() + base::Days(1),
      /*last_access=*/base::Time(), /*last_update=*/base::Time(),
      /*secure=*/false, /*httponly=*/false, CookieSameSite::LAX_MODE,
      COOKIE_PRIORITY_DEFAULT));

  // Add a strict-samesite persistent cookie.
  store_->AddCookie(*CanonicalCookie::CreateUnsafeCookieForTesting(
      kStrictName, kCookieValue, kDomain, kCookiePath,
      /*creation=*/base::Time::Now() - base::Minutes(3),
      /*expiration=*/base::Time::Now() + base::Days(1),
      /*last_access=*/base::Time(), /*last_update=*/base::Time(),
      /*secure=*/false, /*httponly=*/false, CookieSameSite::STRICT_MODE,
      COOKIE_PRIORITY_DEFAULT));

  // Force the store to write its data to the disk.
  DestroyStore();

  // Create a store that loads session cookie and test that the SameSite
  // attribute values are restored.
  CanonicalCookieVector cookies = CreateAndLoad(
      /*crypt_cookies=*/false, /*restore_old_session_cookies=*/true);
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
}

TEST_F(SQLitePersistentCookieStoreTest, SameSiteExtendedTreatedAsUnspecified) {
  constexpr char kDomain[] = "sessioncookie.com";
  constexpr char kExtendedName[] = "extended";
  constexpr char kCookieValue[] = "value";
  constexpr char kCookiePath[] = "/";

  InitializeStore(/*crypt=*/false, /*restore_old_session_cookies=*/true);

  // Add an extended-samesite persistent cookie by first adding a strict-same
  // site cookie, then turning that into the legacy extended-samesite state with
  // direct SQL DB access.
  store_->AddCookie(*CanonicalCookie::CreateUnsafeCookieForTesting(
      kExtendedName, kCookieValue, kDomain, kCookiePath,
      /*creation=*/base::Time::Now() - base::Minutes(1),
      /*expiration=*/base::Time::Now() + base::Days(1),
      /*last_access=*/base::Time(), /*last_update=*/base::Time(),
      /*secure=*/false, /*httponly=*/false, CookieSameSite::STRICT_MODE,
      COOKIE_PRIORITY_DEFAULT));

  // Force the store to write its data to the disk.
  DestroyStore();

  // Open db.
  sql::Database connection;
  ASSERT_TRUE(connection.Open(temp_dir_.GetPath().Append(kCookieFilename)));
  std::string update_stmt(
      "UPDATE cookies SET samesite=3"  // 3 is Extended.
      " WHERE samesite=2"              // 2 is Strict.
  );
  ASSERT_TRUE(connection.Execute(update_stmt));
  connection.Close();

  // Create a store that loads session cookie and test that the
  // SameSite=Extended attribute values is ignored.
  CanonicalCookieVector cookies = CreateAndLoad(
      /*crypt_cookies=*/false, /*restore_old_session_cookies=*/true);
  ASSERT_EQ(1U, cookies.size());

  // Validate that the cookie has the correct SameSite.
  EXPECT_EQ(kExtendedName, cookies[0]->Name());
  EXPECT_EQ(CookieSameSite::UNSPECIFIED, cookies[0]->SameSite());
}

TEST_F(SQLitePersistentCookieStoreTest, SourcePortIsPersistent) {
  const char kDomain[] = "sessioncookie.com";
  const char kCookieValue[] = "value";
  const char kCookiePath[] = "/";

  struct CookieTestValues {
    std::string name;
    int port;
  };

  const std::vector<CookieTestValues> kTestCookies = {
      {"1", 80},
      {"2", 443},
      {"3", 1234},
      {"4", url::PORT_UNSPECIFIED},
      {"5", url::PORT_INVALID}};

  InitializeStore(/*crypt=*/false, /*restore_old_session_cookies=*/true);

  for (const auto& input : kTestCookies) {
    // Add some persistent cookies.
    store_->AddCookie(*CanonicalCookie::CreateUnsafeCookieForTesting(
        input.name, kCookieValue, kDomain, kCookiePath,
        /*creation=*/base::Time::Now() - base::Minutes(1),
        /*expiration=*/base::Time::Now() + base::Days(1),
        /*last_access=*/base::Time(), /*last_update=*/base::Time(),
        /*secure=*/true, /*httponly=*/false, CookieSameSite::LAX_MODE,
        COOKIE_PRIORITY_DEFAULT,
        /*partition_key=*/std::nullopt,
        CookieSourceScheme::kUnset /* Doesn't matter for this test. */,
        input.port));
  }

  // Force the store to write its data to the disk.
  DestroyStore();

  // Create a store that loads session cookie and test that the source_port
  // attribute values are restored.
  CanonicalCookieVector cookies = CreateAndLoad(
      /*crypt_cookies=*/false, /*restore_old_session_cookies=*/true);
  ASSERT_EQ(kTestCookies.size(), cookies.size());

  // Put the cookies into a map, by name, for comparison below.
  std::map<std::string, CanonicalCookie*> cookie_map;
  for (const auto& cookie : cookies)
    cookie_map[cookie->Name()] = cookie.get();

  for (const auto& expected : kTestCookies) {
    ASSERT_EQ(1u, cookie_map.count(expected.name));
    ASSERT_EQ(expected.port, cookie_map[expected.name]->SourcePort());
  }
}

TEST_F(SQLitePersistentCookieStoreTest, UpdateToEncryption) {

  // Create unencrypted cookie store and write something to it.
  InitializeStore(/*crypt=*/false, /*restore_old_session_cookies=*/false);
  AddCookie("name", "value123XYZ", "foo.bar", "/", base::Time::Now());
  DestroyStore();

  // Verify that "value" is visible in the file.  This is necessary in order to
  // have confidence in a later test that "encrypted_value" is not visible.
  std::string contents = ReadRawDBContents();
  EXPECT_NE(0U, contents.length());
  EXPECT_NE(contents.find("value123XYZ"), std::string::npos);

  // Create encrypted cookie store and ensure old cookie still reads.
  CanonicalCookieVector cookies = CreateAndLoad(
      /*crypt_cookies=*/true, /*restore_old_session_cookies=*/false);
  EXPECT_EQ(1U, cookies.size());
  EXPECT_EQ("name", cookies[0]->Name());
  EXPECT_EQ("value123XYZ", cookies[0]->Value());

  // Make sure we can update existing cookie and add new cookie as encrypted.
  store_->DeleteCookie(*(cookies[0]));
  AddCookie("name", "encrypted_value123XYZ", "foo.bar", "/", base::Time::Now());
  AddCookie("other", "something456ABC", "foo.bar", "/",
            base::Time::Now() + base::Microseconds(10));
  DestroyStore();
  cookies = CreateAndLoad(/*crypt_cookies=*/true,
                          /*restore_old_session_cookies=*/false);
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

  // Examine the real record to make sure plaintext version doesn't exist.
  sql::Database db;
  sql::Statement smt;

  ASSERT_TRUE(db.Open(temp_dir_.GetPath().Append(kCookieFilename)));
  smt.Assign(db.GetCachedStatement(SQL_FROM_HERE,
                                   "SELECT * "
                                   "FROM cookies "
                                   "WHERE host_key = 'foo.bar'"));
  int resultcount = 0;
  for (; smt.Step(); ++resultcount) {
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

bool CompareCookies(const std::unique_ptr<CanonicalCookie>& a,
                    const std::unique_ptr<CanonicalCookie>& b) {
  return a->PartialCompare(*b);
}

// Confirm the store can handle having cookies with identical creation
// times stored in it.
TEST_F(SQLitePersistentCookieStoreTest, IdenticalCreationTimes) {
  InitializeStore(/*crypt=*/false, /*restore_old_session_cookies=*/false);
  base::Time cookie_time(base::Time::Now());
  base::Time cookie_expiry(cookie_time + base::Days(1));
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

  CanonicalCookieVector read_in_cookies = CreateAndLoad(
      /*crypt_cookies=*/false, /*restore_old_session_cookies=*/false);
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
  // Note: Cookie encryption is explicitly enabled here to verify threading
  // model with async initialization functions correctly.
  Create(/*crypt_cookies=*/true, /*restore_old_session_cookies=*/false,
         /*use_current_thread=*/true, /*enable_exclusive_access=*/false);

  // Create a cookie on a scheme that doesn't handle cookies by default,
  // and save it.
  std::unique_ptr<CookieMonster> cookie_monster =
      std::make_unique<CookieMonster>(store_.get(), /*net_log=*/nullptr);
  ResultSavingCookieCallback<bool> cookie_scheme_callback1;
  cookie_monster->SetCookieableSchemes({"ftp", "http"},
                                       cookie_scheme_callback1.MakeCallback());
  cookie_scheme_callback1.WaitUntilDone();
  EXPECT_TRUE(cookie_scheme_callback1.result());
  ResultSavingCookieCallback<CookieAccessResult> set_cookie_callback;
  GURL ftp_url("ftp://subdomain.ftperiffic.com/page/");
  auto cookie = CanonicalCookie::CreateForTesting(ftp_url, "A=B; max-age=3600",
                                                  base::Time::Now());
  cookie_monster->SetCanonicalCookieAsync(std::move(cookie), ftp_url,
                                          CookieOptions::MakeAllInclusive(),
                                          set_cookie_callback.MakeCallback());
  set_cookie_callback.WaitUntilDone();
  EXPECT_TRUE(set_cookie_callback.result().status.IsInclude());

  // Also insert a whole bunch of cookies to slow down the background loading of
  // all the cookies.
  for (int i = 0; i < 50; ++i) {
    ResultSavingCookieCallback<CookieAccessResult> set_cookie_callback2;
    GURL url(base::StringPrintf("http://example%d.com/", i));
    auto canonical_cookie = CanonicalCookie::CreateForTesting(
        url, "A=B; max-age=3600", base::Time::Now());
    cookie_monster->SetCanonicalCookieAsync(
        std::move(canonical_cookie), url, CookieOptions::MakeAllInclusive(),
        set_cookie_callback2.MakeCallback());
    set_cookie_callback2.WaitUntilDone();
    EXPECT_TRUE(set_cookie_callback2.result().status.IsInclude());
  }

  net::TestClosure flush_closure;
  cookie_monster->FlushStore(flush_closure.closure());
  flush_closure.WaitForResult();
  cookie_monster = nullptr;

  // Re-create the PersistentCookieStore & CookieMonster. Note that the
  // destroyed store's ops will happen on same runners as the previous
  // instances, so they should complete before the new PersistentCookieStore
  // starts looking at the state on disk.
  Create(/*crypt_cookies=*/true, /*restore_old_session_cookies=*/false,
         /*use_current_thread=*/true, /*enable_exclusive_access=*/false);
  cookie_monster =
      std::make_unique<CookieMonster>(store_.get(), /*net_log=*/nullptr);
  ResultSavingCookieCallback<bool> cookie_scheme_callback2;
  cookie_monster->SetCookieableSchemes({"ftp", "http"},
                                       cookie_scheme_callback2.MakeCallback());
  cookie_scheme_callback2.WaitUntilDone();
  EXPECT_TRUE(cookie_scheme_callback2.result());

  // Now try to get the cookie back.
  GetCookieListCallback get_callback;
  cookie_monster->GetCookieListWithOptionsAsync(
      GURL("ftp://subdomain.ftperiffic.com/page"),
      CookieOptions::MakeAllInclusive(), CookiePartitionKeyCollection(),
      base::BindOnce(&GetCookieListCallback::Run,
                     base::Unretained(&get_callback)));
  get_callback.WaitUntilDone();
  ASSERT_EQ(1u, get_callback.cookies().size());
  EXPECT_EQ("A", get_callback.cookies()[0].Name());
  EXPECT_EQ("B", get_callback.cookies()[0].Value());
  EXPECT_EQ("subdomain.ftperiffic.com", get_callback.cookies()[0].Domain());
}

TEST_F(SQLitePersistentCookieStoreTest, OpsIfInitFailed) {
  // Test to make sure we don't leak pending operations when initialization
  // fails really hard. To inject the failure, we put a directory where the
  // database file ought to be. This test relies on an external leak checker
  // (e.g. lsan) to actual catch thing.
  ASSERT_TRUE(
      base::CreateDirectory(temp_dir_.GetPath().Append(kCookieFilename)));
  Create(/*crypt_cookies=*/false, /*restore_old_session_cookies=*/false,
         /*use_current_thread=*/true,
         /*enable_exclusive_access=*/false);
  std::unique_ptr<CookieMonster> cookie_monster =
      std::make_unique<CookieMonster>(store_.get(), /*net_log=*/nullptr);

  ResultSavingCookieCallback<CookieAccessResult> set_cookie_callback;
  GURL url("http://www.example.com/");
  auto cookie = CanonicalCookie::CreateForTesting(url, "A=B; max-age=3600",
                                                  base::Time::Now());
  cookie_monster->SetCanonicalCookieAsync(std::move(cookie), url,
                                          CookieOptions::MakeAllInclusive(),
                                          set_cookie_callback.MakeCallback());
  set_cookie_callback.WaitUntilDone();
  EXPECT_TRUE(set_cookie_callback.result().status.IsInclude());

  // Things should commit once going out of scope.
  expect_init_errors_ = true;
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

  std::unique_ptr<CanonicalCookie> cookie = CanonicalCookie::CreateForTesting(
      GURL("http://www.example.com/path"), "Tasty=Yes", base::Time::Now());

  for (const TestCase& testcase : testcases) {
    Create(/*crypt_cookies=*/false, /*restore_old_session_cookies=*/false,
           /*use_current_thread=*/true,
           /*enable_exclusive_access=*/false);

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
    DestroyStore();
  }
}

TEST_F(SQLitePersistentCookieStoreTest, NoCoalesceUnrelated) {
  Create(/*crypt_cookies=*/false, /*restore_old_session_cookies=*/false,
         /*use_current_thread=*/true,
         /*enable_exclusive_access=*/false);

  base::RunLoop run_loop;
  store_->Load(base::BindLambdaForTesting(
                   [&](CanonicalCookieVector cookies) { run_loop.Quit(); }),
               NetLogWithSource());
  run_loop.Run();

  std::unique_ptr<CanonicalCookie> cookie1 = CanonicalCookie::CreateForTesting(
      GURL("http://www.example.com/path"), "Tasty=Yes", base::Time::Now());

  std::unique_ptr<CanonicalCookie> cookie2 = CanonicalCookie::CreateForTesting(
      GURL("http://not.example.com/path"), "Tasty=No", base::Time::Now());

  // Wedge the background thread to make sure that it doesn't start consuming
  // the queue.
  background_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&SQLitePersistentCookieStoreTest::WaitOnDBEvent,
                                base::Unretained(this)));

  store_->AddCookie(*cookie1);
  store_->DeleteCookie(*cookie2);
  // delete on cookie2 shouldn't cancel op on unrelated cookie1.
  EXPECT_EQ(2u, store_->GetQueueLengthForTesting());

  db_thread_event_.Signal();
}

// Locking is only supported on Windows.
#if BUILDFLAG(IS_WIN)

class SQLitePersistentCookieStoreExclusiveAccessTest
    : public SQLitePersistentCookieStoreTest,
      public ::testing::WithParamInterface<bool> {
 protected:
  const bool& ShouldBeExclusive() { return GetParam(); }
};

TEST_P(SQLitePersistentCookieStoreExclusiveAccessTest, LockedStore) {
  Create(/*crypt_cookies=*/false, /*restore_old_session_cookies=*/false,
         /*use_current_thread=*/true,
         /*enable_exclusive_access=*/ShouldBeExclusive());

  base::RunLoop run_loop;
  store_->Load(base::BindLambdaForTesting(
                   [&](CanonicalCookieVector cookies) { run_loop.Quit(); }),
               NetLogWithSource());
  run_loop.Run();

  std::unique_ptr<CanonicalCookie> cookie = CanonicalCookie::CreateForTesting(
      GURL("http://www.example.com/path"), "Tasty=Yes", base::Time::Now());

  // Wedge the background thread to make sure that it doesn't start consuming
  // the queue.
  background_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&SQLitePersistentCookieStoreTest::WaitOnDBEvent,
                                base::Unretained(this)));

  store_->AddCookie(*cookie);

  {
    base::File file(
        temp_dir_.GetPath().Append(kCookieFilename),
        base::File::Flags::FLAG_OPEN_ALWAYS | base::File::Flags::FLAG_READ);
    // If locked, should not be able to open file even for read.
    EXPECT_EQ(ShouldBeExclusive(), !file.IsValid());
  }

  db_thread_event_.Signal();
}

TEST_P(SQLitePersistentCookieStoreExclusiveAccessTest, LockedStoreAlreadyOpen) {
  base::HistogramTester histograms;
  base::File file(
      temp_dir_.GetPath().Append(kCookieFilename),
      base::File::Flags::FLAG_CREATE | base::File::Flags::FLAG_READ);
  ASSERT_TRUE(file.IsValid());

  Create(/*crypt_cookies=*/false, /*restore_old_session_cookies=*/false,
         /*use_current_thread=*/true,
         /*enable_exclusive_access=*/ShouldBeExclusive());

  base::RunLoop run_loop;
  store_->Load(base::BindLambdaForTesting(
                   [&](CanonicalCookieVector cookies) { run_loop.Quit(); }),
               NetLogWithSource());
  run_loop.Run();

  // Note: The non-exclusive path is verified in the TearDown for the fixture.
  if (ShouldBeExclusive()) {
    expect_init_errors_ = true;
    histograms.ExpectUniqueSample("Cookie.ErrorInitializeDB",
                                  sql::SqliteLoggedResultCode::kCantOpen, 1);
    histograms.ExpectUniqueSample("Cookie.WinGetLastErrorInitializeDB",
                                  ERROR_SHARING_VIOLATION, 1);
  }
}

INSTANTIATE_TEST_SUITE_P(All,
                         SQLitePersistentCookieStoreExclusiveAccessTest,
                         ::testing::Bool(),
                         [](const auto& info) {
                           return info.param ? "Exclusive" : "NotExclusive";
                         });

#endif  // BUILDFLAG(IS_WIN)

TEST_F(SQLitePersistentCookieStoreTest, CorruptStore) {
  base::HistogramTester histograms;
  base::WriteFile(temp_dir_.GetPath().Append(kCookieFilename),
                  "SQLite format 3 foobarfoobarfoobar");

  Create(/*crypt_cookies=*/false, /*restore_old_session_cookies=*/false,
         /*use_current_thread=*/true,
         /*enable_exclusive_access=*/false);

  base::RunLoop run_loop;
  store_->Load(base::BindLambdaForTesting(
                   [&](CanonicalCookieVector cookies) { run_loop.Quit(); }),
               NetLogWithSource());
  run_loop.Run();

  expect_init_errors_ = true;
  histograms.ExpectUniqueSample("Cookie.ErrorInitializeDB",
                                sql::SqliteLoggedResultCode::kNotADatabase, 1);
}

bool CreateV18Schema(sql::Database* db) {
  sql::MetaTable meta_table;
  if (!meta_table.Init(db, 18, 18)) {
    return false;
  }

  // Version 18 schema
  static constexpr char kCreateSql[] =
      "CREATE TABLE cookies("
      "creation_utc INTEGER NOT NULL,"
      "host_key TEXT NOT NULL,"
      "top_frame_site_key TEXT NOT NULL,"
      "name TEXT NOT NULL,"
      "value TEXT NOT NULL,"
      "encrypted_value BLOB NOT NULL,"
      "path TEXT NOT NULL,"
      "expires_utc INTEGER NOT NULL,"
      "is_secure INTEGER NOT NULL,"
      "is_httponly INTEGER NOT NULL,"
      "last_access_utc INTEGER NOT NULL,"
      "has_expires INTEGER NOT NULL,"
      "is_persistent INTEGER NOT NULL,"
      "priority INTEGER NOT NULL,"
      "samesite INTEGER NOT NULL,"
      "source_scheme INTEGER NOT NULL,"
      "source_port INTEGER NOT NULL,"
      "is_same_party INTEGER NOT NULL,"
      "last_update_utc INTEGER NOT NULL,"
      "UNIQUE (host_key, top_frame_site_key, name, path))";

  static constexpr char kCreateIndexSql[] =
      "CREATE UNIQUE INDEX cookies_unique_index "
      "ON cookies(host_key, top_frame_site_key, name, path)";

  return db->Execute(kCreateSql) && db->Execute(kCreateIndexSql);
}

bool CreateV20Schema(sql::Database* db) {
  sql::MetaTable meta_table;
  if (!meta_table.Init(db, 20, 20)) {
    return false;
  }

  // Version 20 schema
  static constexpr char kCreateSql[] =
      "CREATE TABLE cookies("
      "creation_utc INTEGER NOT NULL,"
      "host_key TEXT NOT NULL,"
      "top_frame_site_key TEXT NOT NULL,"
      "name TEXT NOT NULL,"
      "value TEXT NOT NULL,"
      "encrypted_value BLOB NOT NULL,"
      "path TEXT NOT NULL,"
      "expires_utc INTEGER NOT NULL,"
      "is_secure INTEGER NOT NULL,"
      "is_httponly INTEGER NOT NULL,"
      "last_access_utc INTEGER NOT NULL,"
      "has_expires INTEGER NOT NULL,"
      "is_persistent INTEGER NOT NULL,"
      "priority INTEGER NOT NULL,"
      "samesite INTEGER NOT NULL,"
      "source_scheme INTEGER NOT NULL,"
      "source_port INTEGER NOT NULL,"
      "is_same_party INTEGER NOT NULL,"
      "last_update_utc INTEGER NOT NULL,"
      "UNIQUE (host_key, top_frame_site_key, name, path, source_scheme, "
      "source_port))";

  static constexpr char kCreateIndexSql[] =
      "CREATE UNIQUE INDEX cookies_unique_index "
      "ON cookies(host_key, top_frame_site_key, name, path, source_scheme, "
      "source_port)";

  return db->Execute(kCreateSql) && db->Execute(kCreateIndexSql);
}

bool CreateV21Schema(sql::Database* db) {
  sql::MetaTable meta_table;
  if (!meta_table.Init(db, 21, 21)) {
    return false;
  }

  // Version 21 schema
  static constexpr char kCreateSql[] =
      "CREATE TABLE cookies("
      "creation_utc INTEGER NOT NULL,"
      "host_key TEXT NOT NULL,"
      "top_frame_site_key TEXT NOT NULL,"
      "name TEXT NOT NULL,"
      "value TEXT NOT NULL,"
      "encrypted_value BLOB NOT NULL,"
      "path TEXT NOT NULL,"
      "expires_utc INTEGER NOT NULL,"
      "is_secure INTEGER NOT NULL,"
      "is_httponly INTEGER NOT NULL,"
      "last_access_utc INTEGER NOT NULL,"
      "has_expires INTEGER NOT NULL,"
      "is_persistent INTEGER NOT NULL,"
      "priority INTEGER NOT NULL,"
      "samesite INTEGER NOT NULL,"
      "source_scheme INTEGER NOT NULL,"
      "source_port INTEGER NOT NULL,"
      "last_update_utc INTEGER NOT NULL,"
      "UNIQUE (host_key, top_frame_site_key, name, path, source_scheme, "
      "source_port))";

  static constexpr char kCreateIndexSql[] =
      "CREATE UNIQUE INDEX cookies_unique_index "
      "ON cookies(host_key, top_frame_site_key, name, path, source_scheme, "
      "source_port)";

  return db->Execute(kCreateSql) && db->Execute(kCreateIndexSql);
}

bool CreateV22Schema(sql::Database* db) {
  sql::MetaTable meta_table;
  if (!meta_table.Init(db, 22, 22)) {
    return false;
  }

  // Version 22 schema
  static constexpr char kCreateSql[] =
      "CREATE TABLE cookies("
      "creation_utc INTEGER NOT NULL,"
      "host_key TEXT NOT NULL,"
      "top_frame_site_key TEXT NOT NULL,"
      "name TEXT NOT NULL,"
      "value TEXT NOT NULL,"
      "encrypted_value BLOB NOT NULL,"
      "path TEXT NOT NULL,"
      "expires_utc INTEGER NOT NULL,"
      "is_secure INTEGER NOT NULL,"
      "is_httponly INTEGER NOT NULL,"
      "last_access_utc INTEGER NOT NULL,"
      "has_expires INTEGER NOT NULL,"
      "is_persistent INTEGER NOT NULL,"
      "priority INTEGER NOT NULL,"
      "samesite INTEGER NOT NULL,"
      "source_scheme INTEGER NOT NULL,"
      "source_port INTEGER NOT NULL,"
      "last_update_utc INTEGER NOT NULL,"
      "source_type INTEGER NOT NULL,"
      "UNIQUE (host_key, top_frame_site_key, name, path, source_scheme, "
      "source_port))";

  static constexpr char kCreateIndexSql[] =
      "CREATE UNIQUE INDEX cookies_unique_index "
      "ON cookies(host_key, top_frame_site_key, name, path, source_scheme, "
      "source_port)";

  return db->Execute(kCreateSql) && db->Execute(kCreateIndexSql);
}

bool CreateV23Schema(sql::Database* db) {
  sql::MetaTable meta_table;
  if (!meta_table.Init(db, 23, 23)) {
    return false;
  }

  // Version 23 schema
  static constexpr char kCreateSql[] =
      "CREATE TABLE cookies("
      "creation_utc INTEGER NOT NULL,"
      "host_key TEXT NOT NULL,"
      "top_frame_site_key TEXT NOT NULL,"
      "name TEXT NOT NULL,"
      "value TEXT NOT NULL,"
      "encrypted_value BLOB NOT NULL,"
      "path TEXT NOT NULL,"
      "expires_utc INTEGER NOT NULL,"
      "is_secure INTEGER NOT NULL,"
      "is_httponly INTEGER NOT NULL,"
      "last_access_utc INTEGER NOT NULL,"
      "has_expires INTEGER NOT NULL,"
      "is_persistent INTEGER NOT NULL,"
      "priority INTEGER NOT NULL,"
      "samesite INTEGER NOT NULL,"
      "source_scheme INTEGER NOT NULL,"
      "source_port INTEGER NOT NULL,"
      "last_update_utc INTEGER NOT NULL,"
      "source_type INTEGER NOT NULL,"
      "has_cross_site_ancestor INTEGER NOT NULL);";

  static constexpr char kCreateIndexSql[] =
      "CREATE UNIQUE INDEX cookies_unique_index "
      "ON cookies(host_key, top_frame_site_key, has_cross_site_ancestor, "
      "name, path, source_scheme, source_port)";

  return db->Execute(kCreateSql) && db->Execute(kCreateIndexSql);
}

int GetDBCurrentVersionNumber(sql::Database* db) {
  static constexpr char kGetDBCurrentVersionQuery[] =
      "SELECT value FROM meta WHERE key='version'";
  sql::Statement statement(db->GetUniqueStatement(kGetDBCurrentVersionQuery));
  statement.Step();
  return statement.ColumnInt(0);
}

std::vector<CanonicalCookie> CookiesForMigrationTest() {
  const base::Time now = base::Time::Now();

  std::vector<CanonicalCookie> cookies;
  cookies.push_back(*CanonicalCookie::CreateUnsafeCookieForTesting(
      "A", "B", "example.com", "/", /*creation=*/now, /*expiration=*/now,
      /*last_access=*/now, /*last_update=*/now, /*secure=*/true,
      /*httponly=*/false, CookieSameSite::UNSPECIFIED,
      COOKIE_PRIORITY_DEFAULT));
  cookies.push_back(*CanonicalCookie::CreateUnsafeCookieForTesting(
      "C", "B", "example.com", "/", /*creation=*/now, /*expiration=*/now,
      /*last_access=*/now, /*last_update=*/now, /*secure=*/true,
      /*httponly=*/false, CookieSameSite::UNSPECIFIED,
      COOKIE_PRIORITY_DEFAULT));
  cookies.push_back(*CanonicalCookie::CreateUnsafeCookieForTesting(
      "A", "B", "example2.com", "/", /*creation=*/now, /*expiration=*/now,
      /*last_access=*/now, /*last_update=*/now, /*secure=*/true,
      /*httponly=*/false, CookieSameSite::UNSPECIFIED,
      COOKIE_PRIORITY_DEFAULT));
  cookies.push_back(*CanonicalCookie::CreateUnsafeCookieForTesting(
      "C", "B", "example2.com", "/", /*creation=*/now,
      /*expiration=*/now + base::Days(399), /*last_access=*/now,
      /*last_update=*/now,
      /*secure=*/false, /*httponly=*/false, CookieSameSite::UNSPECIFIED,
      COOKIE_PRIORITY_DEFAULT));
  cookies.push_back(*CanonicalCookie::CreateUnsafeCookieForTesting(
      "A", "B", "example.com", "/path", /*creation=*/now,
      /*expiration=*/now + base::Days(400), /*last_access=*/now,
      /*last_update=*/now,
      /*secure=*/false, /*httponly=*/false, CookieSameSite::UNSPECIFIED,
      COOKIE_PRIORITY_DEFAULT));
  cookies.push_back(*CanonicalCookie::CreateUnsafeCookieForTesting(
      "C", "B", "example.com", "/path", /*creation=*/now,
      /*expiration=*/now + base::Days(401), /*last_access=*/now,
      /*last_update=*/now,
      /*secure=*/false, /*httponly=*/false, CookieSameSite::UNSPECIFIED,
      COOKIE_PRIORITY_DEFAULT));
  cookies.push_back(*CanonicalCookie::CreateUnsafeCookieForTesting(
      "D", "", "empty.com", "/", /*creation=*/now, /*expiration=*/now,
      /*last_access=*/now, /*last_update=*/now, /*secure=*/true,
      /*httponly=*/false, CookieSameSite::UNSPECIFIED,
      COOKIE_PRIORITY_DEFAULT));
  return cookies;
}

// Versions 18, 19, and 20 use the same schema so they can reuse this function.
// AddV20CookiesToDB (and future versions) need to set max_expiration_delta to
// base::Days(400) to simulate expiration limits introduced in version 19.
bool AddV18CookiesToDB(sql::Database* db,
                       base::TimeDelta max_expiration_delta) {
  std::vector<CanonicalCookie> cookies = CookiesForMigrationTest();
  sql::Statement statement(db->GetCachedStatement(
      SQL_FROM_HERE,
      "INSERT INTO cookies (creation_utc, top_frame_site_key, host_key, name, "
      "value, encrypted_value, path, expires_utc, is_secure, is_httponly, "
      "samesite, last_access_utc, has_expires, is_persistent, priority, "
      "source_scheme, source_port, is_same_party, last_update_utc) "
      "VALUES (?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?)"));
  if (!statement.is_valid()) {
    return false;
  }
  sql::Transaction transaction(db);
  if (!transaction.Begin()) {
    return false;
  }
  for (const CanonicalCookie& cookie : cookies) {
    base::Time max_expiration(cookie.CreationDate() + max_expiration_delta);

    statement.Reset(true);
    statement.BindTime(0, cookie.CreationDate());
    // TODO (crbug.com/326605834) Once ancestor chain bit changes are
    // implemented update this method utilize the ancestor bit.
    base::expected<CookiePartitionKey::SerializedCookiePartitionKey,
                   std::string>
        serialized_partition_key =
            CookiePartitionKey::Serialize(cookie.PartitionKey());
    EXPECT_TRUE(serialized_partition_key.has_value());

    statement.BindString(1, serialized_partition_key->TopLevelSite());
    statement.BindString(2, cookie.Domain());
    statement.BindString(3, cookie.Name());
    statement.BindString(4, cookie.Value());
    statement.BindBlob(5, base::span<uint8_t>());  // encrypted_value
    statement.BindString(6, cookie.Path());
    statement.BindTime(7, std::min(cookie.ExpiryDate(), max_expiration));
    statement.BindInt(8, cookie.SecureAttribute());
    statement.BindInt(9, cookie.IsHttpOnly());
    // Note that this, Priority(), and SourceScheme() below nominally rely on
    // the enums in sqlite_persistent_cookie_store.cc having the same values as
    // the ones in ../../cookies/cookie_constants.h.  But nothing in this test
    // relies on that equivalence, so it's not worth the hassle to guarantee
    // that.
    statement.BindInt(10, static_cast<int>(cookie.SameSite()));
    statement.BindTime(11, cookie.LastAccessDate());
    statement.BindInt(12, cookie.IsPersistent());
    statement.BindInt(13, cookie.IsPersistent());
    statement.BindInt(14, static_cast<int>(cookie.Priority()));
    statement.BindInt(15, static_cast<int>(cookie.SourceScheme()));
    statement.BindInt(16, cookie.SourcePort());
    statement.BindInt(17, /*is_same_party=*/false);
    statement.BindTime(18, cookie.LastUpdateDate());
    if (!statement.Run()) {
      return false;
    }
  }
  return transaction.Commit();
}

bool AddV20CookiesToDB(sql::Database* db) {
  return AddV18CookiesToDB(db, base::Days(400));
}

bool AddV21CookiesToDB(sql::Database* db) {
  std::vector<CanonicalCookie> cookies = CookiesForMigrationTest();
  sql::Statement statement(db->GetCachedStatement(
      SQL_FROM_HERE,
      "INSERT INTO cookies (creation_utc, top_frame_site_key, host_key, name, "
      "value, encrypted_value, path, expires_utc, is_secure, is_httponly, "
      "samesite, last_access_utc, has_expires, is_persistent, priority, "
      "source_scheme, source_port, last_update_utc) "
      "VALUES (?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?)"));
  if (!statement.is_valid()) {
    return false;
  }
  sql::Transaction transaction(db);
  if (!transaction.Begin()) {
    return false;
  }
  for (const CanonicalCookie& cookie : cookies) {
    base::Time max_expiration(cookie.CreationDate() + base::Days(400));

    statement.Reset(true);
    statement.BindTime(0, cookie.CreationDate());
    // TODO (crbug.com/326605834) Once ancestor chain bit changes are
    // implemented update this method utilize the ancestor bit.
    base::expected<CookiePartitionKey::SerializedCookiePartitionKey,
                   std::string>
        serialized_partition_key =
            CookiePartitionKey::Serialize(cookie.PartitionKey());
    EXPECT_TRUE(serialized_partition_key.has_value());

    statement.BindString(1, serialized_partition_key->TopLevelSite());
    statement.BindString(2, cookie.Domain());
    statement.BindString(3, cookie.Name());
    statement.BindString(4, cookie.Value());
    statement.BindBlob(5, base::span<uint8_t>());  // encrypted_value
    statement.BindString(6, cookie.Path());
    statement.BindTime(7, std::min(cookie.ExpiryDate(), max_expiration));
    statement.BindInt(8, cookie.SecureAttribute());
    statement.BindInt(9, cookie.IsHttpOnly());
    // Note that this, Priority(), and SourceScheme() below nominally rely on
    // the enums in sqlite_persistent_cookie_store.cc having the same values as
    // the ones in ../../cookies/cookie_constants.h.  But nothing in this test
    // relies on that equivalence, so it's not worth the hassle to guarantee
    // that.
    statement.BindInt(10, static_cast<int>(cookie.SameSite()));
    statement.BindTime(11, cookie.LastAccessDate());
    statement.BindInt(12, cookie.IsPersistent());
    statement.BindInt(13, cookie.IsPersistent());
    statement.BindInt(14, static_cast<int>(cookie.Priority()));
    statement.BindInt(15, static_cast<int>(cookie.SourceScheme()));
    statement.BindInt(16, cookie.SourcePort());
    statement.BindTime(17, cookie.LastUpdateDate());
    if (!statement.Run()) {
      return false;
    }
  }
  return transaction.Commit();
}

bool AddV22CookiesToDB(sql::Database* db,
                       const std::vector<CanonicalCookie>& cookies) {
  sql::Statement statement(db->GetCachedStatement(
      SQL_FROM_HERE,
      "INSERT INTO cookies (creation_utc, top_frame_site_key, host_key, name, "
      "value, encrypted_value, path, expires_utc, is_secure, is_httponly, "
      "samesite, last_access_utc, has_expires, is_persistent, priority, "
      "source_scheme, source_port, last_update_utc, source_type) "
      "VALUES (?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?)"));
  if (!statement.is_valid()) {
    return false;
  }
  sql::Transaction transaction(db);
  if (!transaction.Begin()) {
    return false;
  }
  for (const CanonicalCookie& cookie : cookies) {
    base::Time max_expiration(cookie.CreationDate() + base::Days(400));

    statement.Reset(true);
    statement.BindTime(0, cookie.CreationDate());
    // TODO (crbug.com/326605834) Once ancestor chain bit changes are
    // implemented update this method utilize the ancestor bit.
    base::expected<CookiePartitionKey::SerializedCookiePartitionKey,
                   std::string>
        serialized_partition_key =
            CookiePartitionKey::Serialize(cookie.PartitionKey());
    EXPECT_TRUE(serialized_partition_key.has_value());

    statement.BindString(1, serialized_partition_key->TopLevelSite());
    statement.BindString(2, cookie.Domain());
    statement.BindString(3, cookie.Name());
    statement.BindString(4, cookie.Value());
    statement.BindBlob(5, base::span<uint8_t>());  // encrypted_value
    statement.BindString(6, cookie.Path());
    statement.BindTime(7, std::min(cookie.ExpiryDate(), max_expiration));
    statement.BindInt(8, cookie.SecureAttribute());
    statement.BindInt(9, cookie.IsHttpOnly());
    // Note that this, Priority(), and SourceScheme() below nominally rely on
    // the enums in sqlite_persistent_cookie_store.cc having the same values as
    // the ones in ../../cookies/cookie_constants.h.  But nothing in this test
    // relies on that equivalence, so it's not worth the hassle to guarantee
    // that.
    statement.BindInt(10, static_cast<int>(cookie.SameSite()));
    statement.BindTime(11, cookie.LastAccessDate());
    statement.BindInt(12, cookie.IsPersistent());
    statement.BindInt(13, cookie.IsPersistent());
    statement.BindInt(14, static_cast<int>(cookie.Priority()));
    statement.BindInt(15, static_cast<int>(cookie.SourceScheme()));
    statement.BindInt(16, cookie.SourcePort());
    statement.BindTime(17, cookie.LastUpdateDate());
    statement.BindInt(18, static_cast<int>(cookie.SourceType()));
    if (!statement.Run()) {
      return false;
    }
  }
  return transaction.Commit();
}

bool AddV23CookiesToDB(sql::Database* db,
                       const std::vector<CanonicalCookie>& cookies,
                       CookieCryptoDelegate* crypto,
                       bool place_unencrypted_too) {
  sql::Statement statement(db->GetCachedStatement(
      SQL_FROM_HERE,
      "INSERT INTO cookies (creation_utc, host_key, top_frame_site_key, name, "
      "value, encrypted_value, path, expires_utc, is_secure, is_httponly, "
      "samesite, last_access_utc, has_expires, is_persistent, priority, "
      "source_scheme, source_port, last_update_utc, source_type, "
      "has_cross_site_ancestor) "
      "VALUES (?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?)"));
  if (!statement.is_valid()) {
    return false;
  }
  sql::Transaction transaction(db);
  if (!transaction.Begin()) {
    return false;
  }
  for (const CanonicalCookie& cookie : cookies) {
    base::Time max_expiration(cookie.CreationDate() + base::Days(400));

    statement.Reset(true);
    statement.BindTime(0, cookie.CreationDate());
    // TODO (crbug.com/326605834) Once ancestor chain bit changes are
    // implemented update this method utilize the ancestor bit.
    base::expected<CookiePartitionKey::SerializedCookiePartitionKey,
                   std::string>
        serialized_partition_key =
            CookiePartitionKey::Serialize(cookie.PartitionKey());
    EXPECT_TRUE(serialized_partition_key.has_value());

    statement.BindString(1, cookie.Domain());
    statement.BindString(2, serialized_partition_key->TopLevelSite());
    statement.BindString(3, cookie.Name());
    if (crypto) {
      statement.BindString(
          4, place_unencrypted_too
                 ? cookie.Value()
                 : "");  // value is encrypted. If `place_unencrypted_too` is
                         // set then place it here too, to test bad databases.
      std::string encrypted_value;
      // v23 and below simply encrypted the cookie and stored it in this value.
      EXPECT_TRUE(crypto->EncryptString(cookie.Value(), &encrypted_value));
      statement.BindBlob(5, encrypted_value);  // encrypted_value.
    } else {
      statement.BindString(4, cookie.Value());
      statement.BindBlob(5, base::span<uint8_t>());  // encrypted_value empty.
    }
    statement.BindString(6, cookie.Path());
    statement.BindTime(7, std::min(cookie.ExpiryDate(), max_expiration));
    statement.BindInt(8, cookie.SecureAttribute());
    statement.BindInt(9, cookie.IsHttpOnly());
    // Note that this, Priority(), and SourceScheme() below nominally rely on
    // the enums in sqlite_persistent_cookie_store.cc having the same values as
    // the ones in ../../cookies/cookie_constants.h.  But nothing in this test
    // relies on that equivalence, so it's not worth the hassle to guarantee
    // that.
    statement.BindInt(10, static_cast<int>(cookie.SameSite()));
    statement.BindTime(11, cookie.LastAccessDate());
    statement.BindInt(12, cookie.IsPersistent());
    statement.BindInt(13, cookie.IsPersistent());
    statement.BindInt(14, static_cast<int>(cookie.Priority()));

    // Version 23 updated any preexisting cookies with a source_scheme value of
    // kUnset and a is_secure of true to have a source_scheme value of kSecure.
    // This situation can occur with the test cookies, so update the data to
    // reflect a v23 cookie store.
    auto source_scheme = cookie.SourceScheme();
    if (cookie.SourceScheme() == CookieSourceScheme::kUnset &&
        cookie.IsSecure()) {
      source_scheme = CookieSourceScheme::kSecure;
    }
    statement.BindInt(15, static_cast<int>(source_scheme));
    statement.BindInt(16, cookie.SourcePort());
    statement.BindTime(17, cookie.LastUpdateDate());
    statement.BindInt(18, static_cast<int>(cookie.SourceType()));
    statement.BindBool(19, serialized_partition_key->has_cross_site_ancestor());

    if (!statement.Run()) {
      return false;
    }
  }
  return transaction.Commit();
}

// Confirm the cookie list passed in has the above cookies in it.
void ConfirmCookiesAfterMigrationTest(
    std::vector<std::unique_ptr<CanonicalCookie>> read_in_cookies,
    bool expect_last_update_date = false) {
  ASSERT_EQ(read_in_cookies.size(), 7u);

  std::sort(read_in_cookies.begin(), read_in_cookies.end(), &CompareCookies);
  int i = 0;
  EXPECT_EQ("A", read_in_cookies[i]->Name());
  EXPECT_EQ("B", read_in_cookies[i]->Value());
  EXPECT_EQ("example.com", read_in_cookies[i]->Domain());
  EXPECT_EQ("/", read_in_cookies[i]->Path());
  EXPECT_TRUE(read_in_cookies[i]->SecureAttribute());
  EXPECT_EQ(CookieSourceScheme::kSecure, read_in_cookies[i]->SourceScheme());
  EXPECT_EQ(read_in_cookies[i]->LastUpdateDate(),
            expect_last_update_date ? read_in_cookies[i]->CreationDate()
                                    : base::Time());
  EXPECT_EQ(read_in_cookies[i]->ExpiryDate(),
            read_in_cookies[i]->CreationDate());
  EXPECT_EQ(read_in_cookies[i]->SourceType(), CookieSourceType::kUnknown);

  i++;
  EXPECT_EQ("A", read_in_cookies[i]->Name());
  EXPECT_EQ("B", read_in_cookies[i]->Value());
  EXPECT_EQ("example.com", read_in_cookies[i]->Domain());
  EXPECT_EQ("/path", read_in_cookies[i]->Path());
  EXPECT_FALSE(read_in_cookies[i]->SecureAttribute());
  EXPECT_EQ(CookieSourceScheme::kUnset, read_in_cookies[i]->SourceScheme());
  EXPECT_EQ(read_in_cookies[i]->LastUpdateDate(),
            expect_last_update_date ? read_in_cookies[i]->CreationDate()
                                    : base::Time());
  EXPECT_EQ(read_in_cookies[i]->ExpiryDate(),
            read_in_cookies[i]->CreationDate() + base::Days(400));
  EXPECT_EQ(read_in_cookies[i]->SourceType(), CookieSourceType::kUnknown);

  i++;
  EXPECT_EQ("A", read_in_cookies[i]->Name());
  EXPECT_EQ("B", read_in_cookies[i]->Value());
  EXPECT_EQ("example2.com", read_in_cookies[i]->Domain());
  EXPECT_EQ("/", read_in_cookies[i]->Path());
  EXPECT_TRUE(read_in_cookies[i]->SecureAttribute());
  EXPECT_EQ(CookieSourceScheme::kSecure, read_in_cookies[i]->SourceScheme());
  EXPECT_EQ(read_in_cookies[i]->LastUpdateDate(),
            expect_last_update_date ? read_in_cookies[i]->CreationDate()
                                    : base::Time());
  EXPECT_EQ(read_in_cookies[i]->ExpiryDate(),
            read_in_cookies[i]->CreationDate());
  EXPECT_EQ(read_in_cookies[i]->SourceType(), CookieSourceType::kUnknown);

  i++;
  EXPECT_EQ("C", read_in_cookies[i]->Name());
  EXPECT_EQ("B", read_in_cookies[i]->Value());
  EXPECT_EQ("example.com", read_in_cookies[i]->Domain());
  EXPECT_EQ("/", read_in_cookies[i]->Path());
  EXPECT_TRUE(read_in_cookies[i]->SecureAttribute());
  EXPECT_EQ(CookieSourceScheme::kSecure, read_in_cookies[i]->SourceScheme());
  EXPECT_EQ(read_in_cookies[i]->LastUpdateDate(),
            expect_last_update_date ? read_in_cookies[i]->CreationDate()
                                    : base::Time());
  EXPECT_EQ(read_in_cookies[i]->ExpiryDate(),
            read_in_cookies[i]->CreationDate());
  EXPECT_EQ(read_in_cookies[i]->SourceType(), CookieSourceType::kUnknown);

  i++;
  EXPECT_EQ("C", read_in_cookies[i]->Name());
  EXPECT_EQ("B", read_in_cookies[i]->Value());
  EXPECT_EQ("example.com", read_in_cookies[i]->Domain());
  EXPECT_EQ("/path", read_in_cookies[i]->Path());
  EXPECT_FALSE(read_in_cookies[i]->SecureAttribute());
  EXPECT_EQ(CookieSourceScheme::kUnset, read_in_cookies[i]->SourceScheme());
  EXPECT_EQ(read_in_cookies[i]->LastUpdateDate(),
            expect_last_update_date ? read_in_cookies[i]->CreationDate()
                                    : base::Time());
  // The exact time will be within the last minute due to the cap.
  EXPECT_LE(read_in_cookies[i]->ExpiryDate(),
            base::Time::Now() + base::Days(400));
  EXPECT_GE(read_in_cookies[i]->ExpiryDate(),
            base::Time::Now() + base::Days(400) - base::Minutes(1));
  EXPECT_EQ(read_in_cookies[i]->SourceType(), CookieSourceType::kUnknown);

  i++;
  EXPECT_EQ("C", read_in_cookies[i]->Name());
  EXPECT_EQ("B", read_in_cookies[i]->Value());
  EXPECT_EQ("example2.com", read_in_cookies[i]->Domain());
  EXPECT_EQ("/", read_in_cookies[i]->Path());
  EXPECT_FALSE(read_in_cookies[i]->SecureAttribute());
  EXPECT_EQ(CookieSourceScheme::kUnset, read_in_cookies[i]->SourceScheme());
  EXPECT_EQ(read_in_cookies[i]->LastUpdateDate(),
            expect_last_update_date ? read_in_cookies[i]->CreationDate()
                                    : base::Time());
  EXPECT_EQ(read_in_cookies[i]->ExpiryDate(),
            read_in_cookies[i]->CreationDate() + base::Days(399));
  EXPECT_EQ(read_in_cookies[i]->SourceType(), CookieSourceType::kUnknown);

  i++;
  EXPECT_EQ("D", read_in_cookies[i]->Name());
  EXPECT_EQ("", read_in_cookies[i]->Value());
  EXPECT_EQ("empty.com", read_in_cookies[i]->Domain());
  EXPECT_EQ("/", read_in_cookies[i]->Path());
  EXPECT_TRUE(read_in_cookies[i]->SecureAttribute());
  EXPECT_EQ(CookieSourceScheme::kSecure, read_in_cookies[i]->SourceScheme());
  EXPECT_EQ(read_in_cookies[i]->LastUpdateDate(),
            expect_last_update_date ? read_in_cookies[i]->CreationDate()
                                    : base::Time());
  EXPECT_EQ(read_in_cookies[i]->ExpiryDate(),
            read_in_cookies[i]->CreationDate());
  EXPECT_EQ(read_in_cookies[i]->SourceType(), CookieSourceType::kUnknown);
}

void ConfirmDatabaseVersionAfterMigration(const base::FilePath path,
                                          int version) {
  sql::Database connection;
  ASSERT_TRUE(connection.Open(path));
  ASSERT_GE(GetDBCurrentVersionNumber(&connection), version);
}

TEST_F(SQLitePersistentCookieStoreTest, UpgradeToSchemaVersion19) {
  // Open db.
  const base::FilePath database_path =
      temp_dir_.GetPath().Append(kCookieFilename);
  {
    sql::Database connection;
    ASSERT_TRUE(connection.Open(database_path));
    ASSERT_TRUE(CreateV18Schema(&connection));
    ASSERT_EQ(GetDBCurrentVersionNumber(&connection), 18);
    ASSERT_TRUE(AddV18CookiesToDB(&connection, base::TimeDelta::Max()));
  }

  CanonicalCookieVector read_in_cookies = CreateAndLoad(
      /*crypt_cookies=*/false, /*restore_old_session_cookies=*/false);
  ASSERT_NO_FATAL_FAILURE(
      ConfirmCookiesAfterMigrationTest(std::move(read_in_cookies),
                                       /*expect_last_update_date=*/true));
  DestroyStore();

  ASSERT_NO_FATAL_FAILURE(
      ConfirmDatabaseVersionAfterMigration(database_path, 19));
}

TEST_F(SQLitePersistentCookieStoreTest, UpgradeToSchemaVersion20) {
  // Open db.
  const base::FilePath database_path =
      temp_dir_.GetPath().Append(kCookieFilename);
  {
    sql::Database connection;
    ASSERT_TRUE(connection.Open(database_path));
    // V19's schema is the same as V18, so we can reuse the creation function.
    ASSERT_TRUE(CreateV18Schema(&connection));
    ASSERT_EQ(GetDBCurrentVersionNumber(&connection), 18);
    ASSERT_TRUE(AddV18CookiesToDB(&connection, base::TimeDelta::Max()));
  }

  CanonicalCookieVector read_in_cookies = CreateAndLoad(
      /*crypt_cookies=*/false, /*restore_old_session_cookies=*/false);
  ASSERT_NO_FATAL_FAILURE(
      ConfirmCookiesAfterMigrationTest(std::move(read_in_cookies),
                                       /*expect_last_update_date=*/true));
  DestroyStore();

  ASSERT_NO_FATAL_FAILURE(
      ConfirmDatabaseVersionAfterMigration(database_path, 20));
}

TEST_F(SQLitePersistentCookieStoreTest, UpgradeToSchemaVersion21) {
  // Open db.
  const base::FilePath database_path =
      temp_dir_.GetPath().Append(kCookieFilename);
  {
    sql::Database connection;
    ASSERT_TRUE(connection.Open(database_path));
    ASSERT_TRUE(CreateV20Schema(&connection));
    ASSERT_EQ(GetDBCurrentVersionNumber(&connection), 20);
    ASSERT_TRUE(AddV20CookiesToDB(&connection));
  }

  CanonicalCookieVector read_in_cookies = CreateAndLoad(
      /*crypt_cookies=*/false, /*restore_old_session_cookies=*/false);
  ASSERT_NO_FATAL_FAILURE(
      ConfirmCookiesAfterMigrationTest(std::move(read_in_cookies),
                                       /*expect_last_update_date=*/true));
  DestroyStore();

  ASSERT_NO_FATAL_FAILURE(
      ConfirmDatabaseVersionAfterMigration(database_path, 21));
}

TEST_F(SQLitePersistentCookieStoreTest, UpgradeToSchemaVersion22) {
  // Open db.
  const base::FilePath database_path =
      temp_dir_.GetPath().Append(kCookieFilename);
  {
    sql::Database connection;
    ASSERT_TRUE(connection.Open(database_path));
    ASSERT_TRUE(CreateV21Schema(&connection));
    ASSERT_EQ(GetDBCurrentVersionNumber(&connection), 21);
    ASSERT_TRUE(AddV21CookiesToDB(&connection));
  }

  CanonicalCookieVector read_in_cookies = CreateAndLoad(
      /*crypt_cookies=*/false, /*restore_old_session_cookies=*/false);
  ASSERT_NO_FATAL_FAILURE(
      ConfirmCookiesAfterMigrationTest(std::move(read_in_cookies),
                                       /*expect_last_update_date=*/true));
  DestroyStore();

  ASSERT_NO_FATAL_FAILURE(
      ConfirmDatabaseVersionAfterMigration(database_path, 22));
}

TEST_F(SQLitePersistentCookieStoreTest, UpgradeToSchemaVersion23) {
  // Open db.
  const base::FilePath database_path =
      temp_dir_.GetPath().Append(kCookieFilename);
  {
    sql::Database connection;
    ASSERT_TRUE(connection.Open(database_path));
    ASSERT_TRUE(CreateV22Schema(&connection));
    ASSERT_EQ(GetDBCurrentVersionNumber(&connection), 22);
    ASSERT_TRUE(AddV22CookiesToDB(&connection, CookiesForMigrationTest()));
  }

  CanonicalCookieVector read_in_cookies = CreateAndLoad(
      /*crypt_cookies=*/false, /*restore_old_session_cookies=*/false);
  ASSERT_NO_FATAL_FAILURE(
      ConfirmCookiesAfterMigrationTest(std::move(read_in_cookies),
                                       /*expect_last_update_date=*/true));
  DestroyStore();

  ASSERT_NO_FATAL_FAILURE(
      ConfirmDatabaseVersionAfterMigration(database_path, 23));
}

class SQLitePersistentCookieStorev24UpgradeTest
    : public SQLitePersistentCookieStoreTest,
      public ::testing::WithParamInterface<
          std::tuple</*crypto_for_encrypt*/ bool,
                     /*crypto_for_decrypt*/ bool,
                     /*place_unencrypted_too*/ bool,
                     /*kEncryptedAndPlaintextValuesAreInvalid*/ bool>> {
 protected:
  void SetUp() override {
    features_.InitWithFeatureState(
        features::kEncryptedAndPlaintextValuesAreInvalid,
        std::get<3>(GetParam()));
    SQLitePersistentCookieStoreTest::SetUp();
  }

 private:
  base::test::ScopedFeatureList features_;
};

TEST_P(SQLitePersistentCookieStorev24UpgradeTest, UpgradeToSchemaVersion24) {
  const bool crypto_for_encrypt = std::get<0>(GetParam());
  const bool crypto_for_decrypt = std::get<1>(GetParam());
  const bool place_unencrypted_too = std::get<2>(GetParam());
  const bool drop_dup_values = std::get<3>(GetParam());

  const base::FilePath database_path =
      temp_dir_.GetPath().Append(kCookieFilename);
  {
    sql::Database connection;
    ASSERT_TRUE(connection.Open(database_path));
    ASSERT_TRUE(CreateV23Schema(&connection));
    ASSERT_EQ(GetDBCurrentVersionNumber(&connection), 23);
    auto cryptor = std::make_unique<CookieCryptor>();

    ASSERT_TRUE(AddV23CookiesToDB(&connection, CookiesForMigrationTest(),
                                  crypto_for_encrypt ? cryptor.get() : nullptr,
                                  place_unencrypted_too));
  }
  {
    base::HistogramTester histogram_tester;
    CanonicalCookieVector read_in_cookies = CreateAndLoad(
        /*crypt_cookies=*/crypto_for_decrypt,
        /*restore_old_session_cookies=*/false);

    // If encryption is enabled for encrypt and not available for decrypt, then
    // most cookies will be gone, as the data is encrypted with no way to
    // decrypt.
    if (crypto_for_encrypt && !crypto_for_decrypt) {
      // Subtle: The empty cookie for empty.com will not trigger a cookie load
      // failure. This is because during the migration there is no crypto so no
      // migration occurs for any cookie, including the empty one. Then when
      // attempting to load a v24 store the cookie with an empty value and empty
      // encrypted value will simply load empty.
      EXPECT_EQ(read_in_cookies.size(), 1u);
      // The case of plaintext and encrypted values is always checked when
      // loading a cookie before the availability of crypto. This means the
      // error code here depends on whether migration from v23 to v24 was done
      // with crypto available or not. In this case, crypto was not available
      // during migration so the values were left alone - meaning that if there
      // are both plaintext and encrypted values the
      // kValuesExistInBothEncryptedAndPlaintext error is returned. However, if
      // this cookie does not have both plaintext and encrypted values, then the
      // second check is hit which reports encrypted data that cannot be
      // decrypted - kNoCrypto. Functionality for an already-migrated store (v24
      // and above) with both plaintext and encrypted values is tested in the
      // `OverridePlaintextValue` test below.
      const base::Histogram::Sample expected_bucket =
          drop_dup_values && place_unencrypted_too
              ? /*CookieLoadProblem::kValuesExistInBothEncryptedAndPlaintext*/ 8
              : /*CookieLoadProblem::kNoCrypto*/ 7;
      histogram_tester.ExpectBucketCount("Cookie.LoadProblem", expected_bucket,
                                         CookiesForMigrationTest().size() - 1);
    } else {
      histogram_tester.ExpectTotalCount("Cookie.LoadProblem", 0);
      ASSERT_NO_FATAL_FAILURE(
          ConfirmCookiesAfterMigrationTest(std::move(read_in_cookies),
                                           /*expect_last_update_date=*/true));
    }
    DestroyStore();
  }

  ASSERT_NO_FATAL_FAILURE(
      ConfirmDatabaseVersionAfterMigration(database_path, 24));
}

INSTANTIATE_TEST_SUITE_P(,
                         SQLitePersistentCookieStorev24UpgradeTest,
                         ::testing::Combine(::testing::Bool(),
                                            ::testing::Bool(),
                                            ::testing::Bool(),
                                            ::testing::Bool()));

TEST_F(SQLitePersistentCookieStoreTest, CannotModifyHostName) {
  {
    CreateAndLoad(/*crypt_cookies=*/true,
                  /*restore_old_session_cookies=*/false);
    AddCookie("A", "B", "sensitive.com", "/", base::Time::Now());
    AddCookie("A", "B", "example.com", "/", base::Time::Now());
    DestroyStore();
  }
  {
    const base::FilePath database_path =
        temp_dir_.GetPath().Append(kCookieFilename);
    // Simulate an attacker modifying hostname to attacker controlled, to
    // perform a cookie replay attack.
    sql::Database connection;
    ASSERT_TRUE(connection.Open(database_path));
    sql::Transaction transaction(&connection);
    ASSERT_TRUE(transaction.Begin());
    ASSERT_TRUE(
        connection.Execute("UPDATE cookies SET host_key='attacker.com' WHERE "
                           "host_key='sensitive.com'"));
    ASSERT_TRUE(transaction.Commit());
  }
  {
    base::HistogramTester histogram_tester;
    auto cookies = CreateAndLoad(/*crypt_cookies=*/true,
                                 /*restore_old_session_cookies=*/false);
    // Modified cookie should not load.
    ASSERT_EQ(cookies.size(), 1u);
    ASSERT_EQ(cookies[0]->Domain(), "example.com");
    ASSERT_EQ(cookies[0]->Name(), "A");
    ASSERT_EQ(cookies[0]->Value(), "B");
    DestroyStore();
    histogram_tester.ExpectBucketCount("Cookie.LoadProblem",
                                       /*CookieLoadProblem::kHashFailed*/ 6, 1);
  }
  {
    base::HistogramTester histogram_tester;
    auto cookies = CreateAndLoad(/*crypt_cookies=*/true,
                                 /*restore_old_session_cookies=*/false);
    // Modified cookie should not load.
    ASSERT_EQ(cookies.size(), 1u);
    ASSERT_EQ(cookies[0]->Domain(), "example.com");
    ASSERT_EQ(cookies[0]->Name(), "A");
    ASSERT_EQ(cookies[0]->Value(), "B");
    DestroyStore();
    // The hash failure should only appear once, during the first read, as the
    // invalid cookie gets deleted afterwards.
    histogram_tester.ExpectTotalCount("Cookie.LoadProblem", 0);
  }
}

TEST_F(SQLitePersistentCookieStoreTest, ShortHash) {
  {
    CreateAndLoad(/*crypt_cookies=*/true,
                  /*restore_old_session_cookies=*/false);
    AddCookie("A", "B", "sensitive.com", "/", base::Time::Now());
    AddCookie("A", "B", "example.com", "/", base::Time::Now());
    DestroyStore();
  }
  {
    const base::FilePath database_path =
        temp_dir_.GetPath().Append(kCookieFilename);
    // Simulate an attacker modifying hostname to attacker controlled, to
    // perform a cookie replay attack.
    sql::Database connection;
    ASSERT_TRUE(connection.Open(database_path));
    sql::Transaction transaction(&connection);
    sql::Statement set_encrypted_value(connection.GetUniqueStatement(
        "UPDATE cookies SET encrypted_value=? WHERE host_key='sensitive.com'"));
    CookieCryptor crypto;
    // Short string, without a hash, but valid encryption. This verifies that
    // the decryption code handles short-length encrypted data fine.
    std::string encrypted_data;
    crypto.EncryptString("a", &encrypted_data);
    set_encrypted_value.BindBlob(0, encrypted_data);

    ASSERT_TRUE(transaction.Begin());
    ASSERT_TRUE(set_encrypted_value.Run());
    ASSERT_TRUE(transaction.Commit());
  }
  {
    base::HistogramTester histogram_tester;
    auto cookies = CreateAndLoad(/*crypt_cookies=*/true,
                                 /*restore_old_session_cookies=*/false);
    // Modified cookie should not load.
    ASSERT_EQ(cookies.size(), 1u);
    ASSERT_EQ(cookies[0]->Domain(), "example.com");
    ASSERT_EQ(cookies[0]->Name(), "A");
    ASSERT_EQ(cookies[0]->Value(), "B");
    DestroyStore();
    histogram_tester.ExpectBucketCount("Cookie.LoadProblem",
                                       /*CookieLoadProblem::kHashFailed*/ 6, 1);
  }
  {
    base::HistogramTester histogram_tester;
    auto cookies = CreateAndLoad(/*crypt_cookies=*/true,
                                 /*restore_old_session_cookies=*/false);
    // Modified cookie should not load.
    ASSERT_EQ(cookies.size(), 1u);
    ASSERT_EQ(cookies[0]->Domain(), "example.com");
    ASSERT_EQ(cookies[0]->Name(), "A");
    ASSERT_EQ(cookies[0]->Value(), "B");
    DestroyStore();
    // The hash failure should only appear once, during the first read, as the
    // invalid cookie gets deleted afterwards.
    histogram_tester.ExpectTotalCount("Cookie.LoadProblem", 0);
  }
}

TEST_F(SQLitePersistentCookieStoreTest,
       UpgradeToSchemaVersion23_ConfirmSourceSchemeRecalculation) {
  const base::FilePath database_path =
      temp_dir_.GetPath().Append(kCookieFilename);
  const base::Time now = base::Time::Now();
  std::vector<CanonicalCookie> cookies;

  cookies.push_back(*CanonicalCookie::CreateUnsafeCookieForTesting(
      "secure_true", "A", "example.com", "/", now, now, now, now,
      /*secure=*/true, /*httponly=*/false, CookieSameSite::UNSPECIFIED,
      COOKIE_PRIORITY_DEFAULT, std::optional<CookiePartitionKey>(),
      CookieSourceScheme::kUnset));

  cookies.push_back(*CanonicalCookie::CreateUnsafeCookieForTesting(
      "secure_false", "B", "example.com", "/", now, now, now, now,
      /*secure=*/false, /*httponly=*/false, CookieSameSite::UNSPECIFIED,
      COOKIE_PRIORITY_DEFAULT, std::optional<CookiePartitionKey>(),
      CookieSourceScheme::kUnset));

  // Open database, populate and close db.
  {
    sql::Database db;
    ASSERT_TRUE(db.Open(database_path));
    ASSERT_TRUE(CreateV22Schema(&db));
    ASSERT_EQ(GetDBCurrentVersionNumber(&db), 22);
    ASSERT_TRUE(AddV22CookiesToDB(&db, cookies));
  }

  CanonicalCookieVector read_in_cookies = CreateAndLoad(
      /*crypt_cookies=*/false, /*restore_old_session_cookies=*/true);

  EXPECT_EQ(read_in_cookies.size(), cookies.size());

  // Reopen database for testing.
  sql::Database connection;
  ASSERT_TRUE(connection.Open(database_path));
  ASSERT_GE(GetDBCurrentVersionNumber(&connection), 23);
  for (const auto& cookie : cookies) {
    sql::Statement verify_stmt(connection.GetUniqueStatement(
        "SELECT source_scheme FROM cookies WHERE is_secure=?"));

    verify_stmt.BindBool(0, cookie.SecureAttribute());

    ASSERT_TRUE(verify_stmt.is_valid());

    EXPECT_TRUE(verify_stmt.Step());
    EXPECT_EQ(
        static_cast<int>(cookie.SecureAttribute() ? CookieSourceScheme::kSecure
                                                  : CookieSourceScheme::kUnset),
        verify_stmt.ColumnInt(0));
    // Confirm that exactly one cookie matches the SQL query
    EXPECT_FALSE(verify_stmt.Step());
  }
}

class SQLitePersistentCookieStoreTest_OriginBoundCookies
    : public SQLitePersistentCookieStoreTest {
 public:
  // Creates and stores 4 cookies that differ only by scheme and/or port. When
  // this function returns, the store will be created and all the cookies loaded
  // into cookies_.
  void InitializeTest() {
    InitializeStore(/*crypt=*/false, /*restore_old_session_cookies=*/false);

    basic_cookie_ = CanonicalCookie::CreateForTesting(
        basic_url_, "a=b; max-age=100000", /*creation_time=*/base::Time::Now());

    http_cookie_ = std::make_unique<CanonicalCookie>(*basic_cookie_);
    http_cookie_->SetSourceScheme(CookieSourceScheme::kNonSecure);

    port_444_cookie_ = std::make_unique<CanonicalCookie>(*basic_cookie_);
    port_444_cookie_->SetSourcePort(444);

    http_444_cookie_ = std::make_unique<CanonicalCookie>(*basic_cookie_);
    http_444_cookie_->SetSourceScheme(CookieSourceScheme::kNonSecure);
    http_444_cookie_->SetSourcePort(444);

    store_->AddCookie(*basic_cookie_);
    store_->AddCookie(*http_cookie_);
    store_->AddCookie(*port_444_cookie_);
    store_->AddCookie(*http_444_cookie_);
    // Force the store to write its data to the disk.
    DestroyStore();

    cookies_ = CreateAndLoad(/*crypt_cookies=*/false,
                             /*restore_old_session_cookies=*/false);

    EXPECT_EQ(cookies_.size(), 4UL);
  }

  GURL basic_url_ = GURL("https://example.com");
  std::unique_ptr<net::CanonicalCookie> basic_cookie_;
  std::unique_ptr<net::CanonicalCookie> http_cookie_;
  std::unique_ptr<net::CanonicalCookie> port_444_cookie_;
  std::unique_ptr<net::CanonicalCookie> http_444_cookie_;

  CanonicalCookieVector cookies_;
};

// Tests that cookies which differ only in their scheme and port are considered
// distinct.
TEST_F(SQLitePersistentCookieStoreTest_OriginBoundCookies,
       UniquenessConstraint) {
  InitializeTest();

  // Try to add another cookie that is the same as basic_cookie_ except that its
  // value is different. Value isn't considered as part of the unique constraint
  // and so this cookie won't be considered unique and should fail to be added.
  auto basic_cookie2 =
      CanonicalCookie::CreateForTesting(basic_url_, "a=b2; max-age=100000",
                                        /*creation_time=*/base::Time::Now());

  store_->AddCookie(*basic_cookie2);

  // Force the store to write its data to the disk.
  DestroyStore();

  cookies_.clear();
  cookies_ = CreateAndLoad(/*crypt_cookies=*/false,
                           /*restore_old_session_cookies=*/false);

  // Confirm that basic_cookie2 failed to be added.
  EXPECT_THAT(cookies_, testing::UnorderedElementsAre(
                            MatchesEveryCookieField(*basic_cookie_),
                            MatchesEveryCookieField(*http_cookie_),
                            MatchesEveryCookieField(*port_444_cookie_),
                            MatchesEveryCookieField(*http_444_cookie_)));
}

// Tests that deleting a cookie correctly takes the scheme and port into
// account.
TEST_F(SQLitePersistentCookieStoreTest_OriginBoundCookies, DeleteCookie) {
  InitializeTest();

  // Try to delete just one of the cookies.
  store_->DeleteCookie(*http_444_cookie_);
  DestroyStore();
  cookies_.clear();

  cookies_ = CreateAndLoad(/*crypt_cookies=*/false,
                           /*restore_old_session_cookies=*/false);

  // Only the single cookie should be deleted.
  EXPECT_THAT(cookies_, testing::UnorderedElementsAre(
                            MatchesEveryCookieField(*basic_cookie_),
                            MatchesEveryCookieField(*http_cookie_),
                            MatchesEveryCookieField(*port_444_cookie_)));
}

// Tests that updating a cookie correctly takes the scheme and port into
// account.
TEST_F(SQLitePersistentCookieStoreTest_OriginBoundCookies,
       UpdateCookieAccessTime) {
  InitializeTest();

  base::Time basic_last_access = basic_cookie_->LastAccessDate();
  base::Time http_last_access = http_cookie_->LastAccessDate();
  base::Time port_444_last_access = port_444_cookie_->LastAccessDate();
  base::Time http_444_last_access = http_444_cookie_->LastAccessDate();

  base::Time new_last_access = http_444_last_access + base::Hours(1);
  http_444_cookie_->SetLastAccessDate(new_last_access);

  store_->UpdateCookieAccessTime(*http_444_cookie_);
  DestroyStore();
  cookies_.clear();

  cookies_ = CreateAndLoad(/*crypt_cookies=*/false,
                           /*restore_old_session_cookies=*/false);

  // All loaded cookies' should have their original LastAccessDate() except for
  // the one updated to new_last_access.
  EXPECT_THAT(
      cookies_,
      testing::UnorderedElementsAre(
          MatchesCookieKeyAndLastAccessDate(basic_cookie_->StrictlyUniqueKey(),
                                            basic_last_access),
          MatchesCookieKeyAndLastAccessDate(http_cookie_->StrictlyUniqueKey(),
                                            http_last_access),
          MatchesCookieKeyAndLastAccessDate(
              port_444_cookie_->StrictlyUniqueKey(), port_444_last_access),
          MatchesCookieKeyAndLastAccessDate(
              http_444_cookie_->StrictlyUniqueKey(), new_last_access)));
}

TEST_F(SQLitePersistentCookieStoreTest, SavingPartitionedCookies) {
  InitializeStore(/*crypt=*/false, /*restore_old_session_cookies=*/false);

  store_->AddCookie(*CanonicalCookie::CreateUnsafeCookieForTesting(
      "__Host-foo", "bar", GURL("https://example.com/").host(), "/",
      /*creation=*/base::Time::Now(),
      /*expiration=*/base::Time::Now() + base::Days(1),
      /*last_access=*/base::Time::Now(),
      /*last_update=*/base::Time::Now(), /*secure=*/true, /*httponly=*/false,
      CookieSameSite::UNSPECIFIED, COOKIE_PRIORITY_DEFAULT,
      CookiePartitionKey::FromURLForTesting(GURL("https://toplevelsite.com"))));
  Flush();

  std::string got_db_content(ReadRawDBContents());
  EXPECT_NE(got_db_content.find("__Host-foo"), std::string::npos);

  DestroyStore();
}

TEST_F(SQLitePersistentCookieStoreTest, LoadingPartitionedCookies) {
  InitializeStore(/*crypt=*/false, /*restore_old_session_cookies=*/false);
  DestroyStore();

  // Insert a partitioned cookie into the database manually.
  base::FilePath store_name(temp_dir_.GetPath().Append(kCookieFilename));
  std::unique_ptr<sql::Database> db(std::make_unique<sql::Database>());
  ASSERT_TRUE(db->Open(store_name));

  sql::Statement stmt(db->GetUniqueStatement(
      "INSERT INTO cookies (creation_utc, host_key, top_frame_site_key, name, "
      "value, encrypted_value, path, expires_utc, is_secure, is_httponly, "
      "samesite, last_access_utc, has_expires, is_persistent, priority, "
      "source_scheme, source_port, last_update_utc, source_type, "
      "has_cross_site_ancestor) "
      "VALUES (?,?,?,?,?,'',?,?,1,0,0,?,1,1,0,?,?,?,0, 1)"));
  ASSERT_TRUE(stmt.is_valid());

  base::Time creation(base::Time::Now());
  base::Time expiration(creation + base::Days(1));
  base::Time last_access(base::Time::Now());
  base::Time last_update(base::Time::Now());

  stmt.BindTime(0, creation);
  stmt.BindString(1, GURL("https://www.example.com/").host());
  stmt.BindString(2, "https://toplevelsite.com");
  stmt.BindString(3, "__Host-foo");
  stmt.BindString(4, "bar");
  stmt.BindString(5, "/");
  stmt.BindTime(6, expiration);
  stmt.BindTime(7, last_access);
  stmt.BindInt(8, static_cast<int>(CookieSourceScheme::kUnset));
  stmt.BindInt(9, SQLitePersistentCookieStore::kDefaultUnknownPort);
  stmt.BindTime(10, last_update);
  ASSERT_TRUE(stmt.Run());
  stmt.Clear();
  db.reset();

  CanonicalCookieVector cookies = CreateAndLoad(
      /*crypt_cookies=*/false, /*restore_old_session_cookies=*/false);

  EXPECT_EQ(1u, cookies.size());
  auto cc = std::move(cookies[0]);
  EXPECT_EQ("__Host-foo", cc->Name());
  EXPECT_EQ("bar", cc->Value());
  EXPECT_EQ(GURL("https://www.example.com/").host(), cc->Domain());
  EXPECT_TRUE(cc->IsPartitioned());
  EXPECT_EQ(
      CookiePartitionKey::FromURLForTesting(GURL("https://toplevelsite.com")),
      cc->PartitionKey());
  EXPECT_EQ(last_update, cc->LastUpdateDate());
}

std::unique_ptr<CanonicalCookie> CreatePartitionedCookie(
    const std::string& name,
    const std::string& domain,
    const std::string& top_frame_site_key,
    CookiePartitionKey::AncestorChainBit ancestor_chain_bit,
    CookieSourceScheme scheme = CookieSourceScheme::kUnset,
    bool partitioned_cookies_enabled = true) {
  const base::Time now = base::Time::Now();

  return CanonicalCookie::CreateUnsafeCookieForTesting(
      name, "B", domain, "/", now, now, now, now, /*secure=*/true,
      /*httponly=*/false, CookieSameSite::UNSPECIFIED, COOKIE_PRIORITY_DEFAULT,
      partitioned_cookies_enabled
          ? CookiePartitionKey::FromURLForTesting(GURL(top_frame_site_key),
                                                  ancestor_chain_bit)
          :
          /* std::nullopt can't be used because of the ternary evaluation might
           result in different types */
          std::optional<CookiePartitionKey>(),
      scheme);
}

// Pairs contain a cookie, and hard coded value for has_cross_site_ancestor
// value.
//
// During migration we have no way of knowing if a cross site ancestor was
// present. When the existing domain and the top_level_site of the partition key
// are the same. The default behavior is to set the cross site value to
// kSameSite, so ignore the kCrossSite cookie when testing migration.
std::vector<std::pair<CanonicalCookie, std::string>>
GenerateHasCrossSiteAncestorCookiesAndVals(bool migrating = false) {
  std::vector<std::pair<CanonicalCookie, std::string>> results;
  const std::string default_domain = "example.com";

  // Key and domain are the same site
  results.emplace_back(
      *CreatePartitionedCookie("A", default_domain, "https://www.example.com",
                               CookiePartitionKey::AncestorChainBit::kSameSite),
      "0");
  if (!migrating) {
    // Key and domain are the same site but with kCrossSite
    results.emplace_back(*CreatePartitionedCookie(
                             "B", default_domain, "https://www.example.com",
                             CookiePartitionKey::AncestorChainBit::kCrossSite),
                         "1");
  }
  // Key and domain are different
  results.emplace_back(*CreatePartitionedCookie(
                           "C", default_domain, "https://www.toplevelsite.com",
                           CookiePartitionKey::AncestorChainBit::kCrossSite),
                       "1");
  // Domain is a substring
  results.emplace_back(*CreatePartitionedCookie(
                           "D", "ample.com", "https://www.example.com",
                           CookiePartitionKey::AncestorChainBit::kCrossSite),
                       "1");
  // http check kNonSecure scheme match.
  results.emplace_back(
      *CreatePartitionedCookie("E", default_domain, "http://www.example.com",
                               CookiePartitionKey::AncestorChainBit::kSameSite),
      "0");

  return results;
}

TEST_F(SQLitePersistentCookieStoreTest,
       UpgradeToSchemaVersion23_AddingHasCrossSiteAncestor) {
  const base::FilePath database_path =
      temp_dir_.GetPath().Append(kCookieFilename);

  std::vector<std::pair<CanonicalCookie, std::string>>
      cookies_and_expected_values =
          GenerateHasCrossSiteAncestorCookiesAndVals(true);

  std::vector<CanonicalCookie> cookies;
  for (auto cookie_pair : cookies_and_expected_values) {
    cookies.push_back(cookie_pair.first);
  }
  // Open database, populate and close db.
  {
    sql::Database db;
    ASSERT_TRUE(db.Open(database_path));
    ASSERT_TRUE(CreateV22Schema(&db));
    ASSERT_EQ(GetDBCurrentVersionNumber(&db), 22);
    ASSERT_TRUE(AddV22CookiesToDB(&db, cookies));
  }

  CanonicalCookieVector read_in_cookies = CreateAndLoad(
      /*crypt_cookies=*/false, /*restore_old_session_cookies=*/true);

  EXPECT_EQ(read_in_cookies.size(), cookies.size());

  // Reopen database for testing.
  sql::Database connection;
  ASSERT_TRUE(connection.Open(database_path));
  ASSERT_GE(GetDBCurrentVersionNumber(&connection), 23);

  for (const auto& cookie_pair : cookies_and_expected_values) {
    // TODO (crbug.com/326605834) Once ancestor chain bit changes are
    // implemented update this method utilize the ancestor bit.
    base::expected<CookiePartitionKey::SerializedCookiePartitionKey,
                   std::string>
        serialized_partition_key =
            CookiePartitionKey::Serialize(cookie_pair.first.PartitionKey());
    ASSERT_TRUE(serialized_partition_key.has_value());

    sql::Statement verify_stmt(connection.GetUniqueStatement(
        "SELECT name FROM cookies WHERE host_key=?"
        " AND top_frame_site_key=?"
        " AND has_cross_site_ancestor=?"));

    verify_stmt.BindString(0, cookie_pair.first.Domain());
    verify_stmt.BindString(1, serialized_partition_key->TopLevelSite());
    verify_stmt.BindString(2, cookie_pair.second);

    ASSERT_TRUE(verify_stmt.is_valid());
    EXPECT_TRUE(verify_stmt.Step());
    EXPECT_EQ(cookie_pair.first.Name(), verify_stmt.ColumnString(0));
    // Confirm that exactly one cookie matches the SQL query
    EXPECT_FALSE(verify_stmt.Step());
  }
}

TEST_F(SQLitePersistentCookieStoreTest,
       TestValueOfHasCrossSiteAncestorOnDoCommit) {
  InitializeStore(/*crypt=*/false, /*restore_old_session_cookies=*/false);

  std::vector<std::pair<CanonicalCookie, std::string>>
      cookies_and_expected_values =
          GenerateHasCrossSiteAncestorCookiesAndVals();
  for (const auto& cookie_pair : cookies_and_expected_values) {
    store_->AddCookie(cookie_pair.first);
  }

  // Force the store to write its data to the disk.
  DestroyStore();

  cookies_ = CreateAndLoad(/*crypt_cookies=*/false,
                           /*restore_old_session_cookies=*/false);
  EXPECT_EQ(cookies_.size(), cookies_and_expected_values.size());

  sql::Database connection;
  ASSERT_TRUE(connection.Open(temp_dir_.GetPath().Append(kCookieFilename)));
  ASSERT_GT(GetDBCurrentVersionNumber(&connection), 23);

  for (const auto& cookie_pair : cookies_and_expected_values) {
    // TODO (crbug.com/326605834) Once ancestor chain bit changes are
    // implemented update this method utilize the ancestor bit.
    base::expected<CookiePartitionKey::SerializedCookiePartitionKey,
                   std::string>
        serialized_partition_key =
            CookiePartitionKey::Serialize(cookie_pair.first.PartitionKey());
    ASSERT_TRUE(serialized_partition_key.has_value());

    sql::Statement verify_stmt(connection.GetUniqueStatement(
        "SELECT name FROM cookies WHERE host_key=?"
        " AND top_frame_site_key=?"
        " AND has_cross_site_ancestor=?"));

    verify_stmt.BindString(0, cookie_pair.first.Domain());
    verify_stmt.BindString(1, serialized_partition_key->TopLevelSite());
    verify_stmt.BindString(2, cookie_pair.second);
    ASSERT_TRUE(verify_stmt.is_valid());

    EXPECT_TRUE(verify_stmt.Step());
    EXPECT_EQ(cookie_pair.first.Name(), verify_stmt.ColumnString(0));
    // Confirm that exactly one cookie matches the SQL query
    EXPECT_FALSE(verify_stmt.Step());
  }
}

TEST_F(SQLitePersistentCookieStoreTest, NoCryptoForDecryption) {
  InitializeStore(/*crypt=*/true, /*restore_old_session_cookies=*/false);
  AddCookie("X", "Y", "foo.bar", "/", base::Time::Now());
  DestroyStore();

  {
    base::HistogramTester histogram_tester;
    const auto cookies =
        CreateAndLoad(/*crypt=*/false, /*restore_old_session_cookies=*/false);
    ASSERT_TRUE(cookies.empty());
    histogram_tester.ExpectBucketCount("Cookie.LoadProblem",
                                       /*CookieLoadProblem::kNoCrypto*/ 7, 1);
  }
}

class SQLitePersistentCookieStoreTestWithDropDupDataFeature
    : public ::testing::WithParamInterface<
          /*features::kEncryptedAndPlaintextValuesAreInvalid*/ bool>,
      public SQLitePersistentCookieStoreTest {
 public:
  void SetUp() override {
    features_.InitWithFeatureState(
        features::kEncryptedAndPlaintextValuesAreInvalid,
        IsDroppingCookiesEnabled());
    SQLitePersistentCookieStoreTest::SetUp();
  }

 protected:
  bool IsDroppingCookiesEnabled() const { return GetParam(); }

 private:
  base::test::ScopedFeatureList features_;
};

// This test verifies that if a plaintext value is in the store (e.g. written in
// manually, or crypto was at some point not available in the past) and crypto
// is now available, it can still be read fine, including if the value is empty.
// It also tests the case where both a plaintext and encrypted value exist,
// where the encrypted value should always take precedence except if
// kEncryptedAndPlaintextValuesAreInvalid is enabled, in which case the cookie
// is dropped.
TEST_P(SQLitePersistentCookieStoreTestWithDropDupDataFeature,
       OverridePlaintextValue) {
  {
    CreateAndLoad(/*crypt_cookies=*/true,
                  /*restore_old_session_cookies=*/false);
    AddCookie("A", "B", "example.com", "/", base::Time::Now());
    AddCookie("C", "D", "example2.com", "/", base::Time::Now());
    AddCookie("E", "F", "example3.com", "/", base::Time::Now());
    DestroyStore();
  }
  {
    base::HistogramTester histogram_tester;
    auto cookies = CreateAndLoad(/*crypt_cookies=*/true,
                                 /*restore_old_session_cookies=*/false);
    ASSERT_EQ(cookies.size(), 3u);
    EXPECT_EQ(cookies[0]->Domain(), "example.com");
    EXPECT_EQ(cookies[0]->Name(), "A");
    EXPECT_EQ(cookies[0]->Value(), "B");
    EXPECT_EQ(cookies[1]->Domain(), "example2.com");
    EXPECT_EQ(cookies[1]->Name(), "C");
    EXPECT_EQ(cookies[1]->Value(), "D");
    EXPECT_EQ(cookies[2]->Domain(), "example3.com");
    EXPECT_EQ(cookies[2]->Name(), "E");
    EXPECT_EQ(cookies[2]->Value(), "F");
    DestroyStore();
  }
  {
    const base::FilePath database_path =
        temp_dir_.GetPath().Append(kCookieFilename);
    sql::Database connection;
    ASSERT_TRUE(connection.Open(database_path));
    sql::Transaction transaction(&connection);
    ASSERT_TRUE(transaction.Begin());
    // Clear the encrypted value and set the plaintext value to something else.
    ASSERT_TRUE(
        connection.Execute("UPDATE cookies SET encrypted_value=x'', "
                           "value='Val' WHERE host_key='example.com'"));
    // Verify also that an empty value can be injected.
    ASSERT_TRUE(
        connection.Execute("UPDATE cookies SET encrypted_value=x'', "
                           "value='' WHERE host_key='example2.com'"));
    // Verify if both are present, it's dealt with correctly (encrypted data
    // takes priority), and a histogram is recorded.
    ASSERT_TRUE(connection.Execute(
        "UPDATE cookies SET value='not-F' WHERE host_key='example3.com'"));
    ASSERT_TRUE(transaction.Commit());
  }
  {
    base::HistogramTester histogram_tester;
    auto cookies = CreateAndLoad(/*crypt_cookies=*/true,
                                 /*restore_old_session_cookies=*/false);
    histogram_tester.ExpectBucketCount("Cookie.EncryptedAndPlaintextValues",
                                       true, 1);

    // Third cookie (example3.com) should be dropped if
    // kEncryptedAndPlaintextValuesAreInvalid is enabled.
    ASSERT_EQ(cookies.size(), IsDroppingCookiesEnabled() ? 2u : 3u);
    // Cookie should load fine since it's been modified by writing plaintext and
    // clearing ciphertext.
    EXPECT_EQ(cookies[0]->Domain(), "example.com");
    EXPECT_EQ(cookies[0]->Name(), "A");
    EXPECT_EQ(cookies[0]->Value(), "Val");
    EXPECT_EQ(cookies[1]->Domain(), "example2.com");
    EXPECT_EQ(cookies[1]->Name(), "C");
    EXPECT_TRUE(cookies[1]->Value().empty());

    if (IsDroppingCookiesEnabled()) {
      // Cookie should be dropped and a metric recorded.
      histogram_tester.ExpectBucketCount(
          "Cookie.LoadProblem",
          /*CookieLoadProblem::kValuesExistInBothEncryptedAndPlaintext*/ 8, 1u);
    } else {
      // If the kEncryptedAndPlaintextValuesAreInvalid feature is disabled (and
      // the cookie was not dropped) then the final cookie should always use the
      // encrypted value and not the plaintext value.
      EXPECT_EQ(cookies[2]->Domain(), "example3.com");
      EXPECT_EQ(cookies[2]->Name(), "E");
      EXPECT_EQ(cookies[2]->Value(), "F");
      histogram_tester.ExpectTotalCount("Cookie.LoadProblem", 0);
    }
    DestroyStore();
  }
}

INSTANTIATE_TEST_SUITE_P(,
                         SQLitePersistentCookieStoreTestWithDropDupDataFeature,
                         ::testing::Bool(),
                         [](auto& info) {
                           return info.param ? "Enabled" : "Disabled";
                         });

}  // namespace net
