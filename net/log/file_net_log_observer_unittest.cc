// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40284755): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

#include "net/log/file_net_log_observer.h"

#include <string>
#include <vector>

#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_file.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#include "base/test/gmock_expected_support.h"
#include "base/threading/thread.h"
#include "base/types/expected.h"
#include "base/types/expected_macros.h"
#include "base/values.h"
#include "build/build_config.h"
#include "net/base/test_completion_callback.h"
#include "net/log/net_log.h"
#include "net/log/net_log_entry.h"
#include "net/log/net_log_event_type.h"
#include "net/log/net_log_source.h"
#include "net/log/net_log_source_type.h"
#include "net/log/net_log_util.h"
#include "net/log/net_log_values.h"
#include "net/test/test_with_task_environment.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

namespace {

// Indicates the number of event files used in test cases.
const int kTotalNumFiles = 10;

// Used to set the total file size maximum in test cases where the file size
// doesn't matter.
const int kLargeFileSize = 100000000;

// Used to set the size of events to be sent to the observer in test cases
// where event size doesn't matter.
const size_t kDummyEventSize = 150;

// Adds |num_entries| to |logger|. The "inverse" of this is VerifyEventsInLog().
void AddEntries(FileNetLogObserver* logger,
                int num_entries,
                size_t entry_size) {
  // Get base size of event.
  const int kDummyId = 0;
  NetLogSource source(NetLogSourceType::HTTP2_SESSION, kDummyId);
  NetLogEntry base_entry(NetLogEventType::PAC_JAVASCRIPT_ERROR, source,
                         NetLogEventPhase::BEGIN, base::TimeTicks::Now(),
                         NetLogParamsWithString("message", ""));
  base::Value::Dict value = base_entry.ToDict();
  std::string json;
  base::JSONWriter::Write(value, &json);
  size_t base_entry_size = json.size();

  // The maximum value of base::TimeTicks::Now() will be the maximum value of
  // int64_t, and if the maximum number of digits are included, the
  // |base_entry_size| could be up to 136 characters. Check that the event
  // format does not include additional padding.
  DCHECK_LE(base_entry_size, 136u);

  // |entry_size| should be at least as big as the largest possible base
  // entry.
  EXPECT_GE(entry_size, 136u);

  // |entry_size| cannot be smaller than the minimum event size.
  EXPECT_GE(entry_size, base_entry_size);

  for (int i = 0; i < num_entries; i++) {
    source = NetLogSource(NetLogSourceType::HTTP2_SESSION, i);
    std::string id = base::NumberToString(i);

    // String size accounts for the number of digits in id so that all events
    // are the same size.
    std::string message =
        std::string(entry_size - base_entry_size - id.size() + 1, 'x');
    NetLogEntry entry(NetLogEventType::PAC_JAVASCRIPT_ERROR, source,
                      NetLogEventPhase::BEGIN, base::TimeTicks::Now(),
                      NetLogParamsWithString("message", message));
    logger->OnAddEntry(entry);
  }
}

// ParsedNetLog holds the parsed contents of a NetLog file (constants, events,
// and polled data).
struct ParsedNetLog {
  base::expected<void, std::string> InitFromFileContents(
      const std::string& input);
  const base::Value::Dict* GetEvent(size_t i) const;

  // Initializes the ParsedNetLog by parsing a JSON file.
  // Owner for the Value tree and a dictionary for the entire netlog.
  base::Value root;

  // The constants dictionary.
  raw_ptr<const base::Value::Dict> constants = nullptr;

  // The events list.
  raw_ptr<const base::Value::List> events = nullptr;

  // The optional polled data (may be nullptr).
  raw_ptr<const base::Value::Dict> polled_data = nullptr;
};

base::expected<void, std::string> ParsedNetLog::InitFromFileContents(
    const std::string& input) {
  if (input.empty()) {
    return base::unexpected("input is empty");
  }

  ASSIGN_OR_RETURN(root, base::JSONReader::ReadAndReturnValueWithError(input),
                   &base::JSONReader::Error::message);

  const base::Value::Dict* dict = root.GetIfDict();
  if (!dict) {
    return base::unexpected("Not a dictionary");
  }

  events = dict->FindListByDottedPath("events");
  if (!events) {
    return base::unexpected("No events list");
  }

  constants = dict->FindDictByDottedPath("constants");
  if (!constants) {
    return base::unexpected("No constants dictionary");
  }

  // Polled data is optional (ignore success).
  polled_data = dict->FindDictByDottedPath("polledData");

  return base::ok();
}

// Returns the event at index |i|, or nullptr if there is none.
const base::Value::Dict* ParsedNetLog::GetEvent(size_t i) const {
  if (!events || i >= events->size())
    return nullptr;

  return (*events)[i].GetIfDict();
}

// Creates a ParsedNetLog by reading a NetLog from a file. Returns nullptr on
// failure.
base::expected<std::unique_ptr<ParsedNetLog>, std::string> ReadNetLogFromDisk(
    const base::FilePath& log_path) {
  std::string input;
  if (!base::ReadFileToString(log_path, &input)) {
    return base::unexpected("Failed reading file: " +
                            base::UTF16ToUTF8(log_path.LossyDisplayName()));
  }

  std::unique_ptr<ParsedNetLog> result = std::make_unique<ParsedNetLog>();

  RETURN_IF_ERROR(result->InitFromFileContents(input));
  return result;
}

// Checks that |log| contains events as emitted by AddEntries() above.
// |num_events_emitted| corresponds to |num_entries| of AddEntries(). Whereas
// |num_events_saved| is the expected number of events that have actually been
// written to the log (post-truncation).
void VerifyEventsInLog(const ParsedNetLog* log,
                       size_t num_events_emitted,
                       size_t num_events_saved) {
  ASSERT_TRUE(log);
  ASSERT_LE(num_events_saved, num_events_emitted);
  ASSERT_EQ(num_events_saved, log->events->size());

  // The last |num_events_saved| should all be sequential, with the last one
  // being numbered |num_events_emitted - 1|.
  for (size_t i = 0; i < num_events_saved; ++i) {
    const base::Value::Dict* event = log->GetEvent(i);
    ASSERT_TRUE(event);

    size_t expected_source_id = num_events_emitted - num_events_saved + i;

    std::optional<int> id_value = event->FindIntByDottedPath("source.id");
    ASSERT_EQ(static_cast<int>(expected_source_id), id_value);
  }
}

// Helper that checks whether |dict| has a string property at |key| having
// |value|.
void ExpectDictionaryContainsProperty(const base::Value::Dict& dict,
                                      const std::string& key,
                                      const std::string& value) {
  const std::string* actual_value = dict.FindStringByDottedPath(key);
  ASSERT_EQ(value, *actual_value);
}

// Used for tests that are common to both bounded and unbounded modes of the
// the FileNetLogObserver. The param is true if bounded mode is used.
class FileNetLogObserverTest : public ::testing::TestWithParam<bool>,
                               public WithTaskEnvironment {
 public:
  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    log_path_ = temp_dir_.GetPath().AppendASCII("net-log.json");
  }

  void TearDown() override {
    logger_.reset();
    // FileNetLogObserver destructor might post to message loop.
    RunUntilIdle();
  }

  bool IsBounded() const { return GetParam(); }

  void CreateAndStartObserving(
      std::unique_ptr<base::Value::Dict> constants,
      NetLogCaptureMode capture_mode = NetLogCaptureMode::kDefault) {
    if (IsBounded()) {
      logger_ = FileNetLogObserver::CreateBoundedForTests(
          log_path_, kLargeFileSize, kTotalNumFiles, capture_mode,
          std::move(constants));
    } else {
      logger_ = FileNetLogObserver::CreateUnbounded(log_path_, capture_mode,
                                                    std::move(constants));
    }

    logger_->StartObserving(NetLog::Get());
  }

  void CreateAndStartObservingBoundedFile(
      int max_file_size,
      std::unique_ptr<base::Value::Dict> constants) {
    base::File file(log_path_,
                    base::File::FLAG_CREATE | base::File::FLAG_WRITE);
    EXPECT_TRUE(file.IsValid());
    // Stick in some nonsense to make sure the file gets cleared properly
    file.Write(0, "not json", 8);

    logger_ = FileNetLogObserver::CreateBoundedFile(
        std::move(file), max_file_size, NetLogCaptureMode::kDefault,
        std::move(constants));

    logger_->StartObserving(NetLog::Get());
  }

  void CreateAndStartObservingPreExisting(
      std::unique_ptr<base::Value::Dict> constants) {
    ASSERT_TRUE(scratch_dir_.CreateUniqueTempDir());

    base::File file(log_path_,
                    base::File::FLAG_CREATE | base::File::FLAG_WRITE);
    EXPECT_TRUE(file.IsValid());
    // Stick in some nonsense to make sure the file gets cleared properly
    file.Write(0, "not json", 8);

    if (IsBounded()) {
      logger_ = FileNetLogObserver::CreateBoundedPreExisting(
          scratch_dir_.GetPath(), std::move(file), kLargeFileSize,
          NetLogCaptureMode::kDefault, std::move(constants));
    } else {
      logger_ = FileNetLogObserver::CreateUnboundedPreExisting(
          std::move(file), NetLogCaptureMode::kDefault, std::move(constants));
    }

    logger_->StartObserving(NetLog::Get());
  }

  bool LogFileExists() {
    // The log files are written by a sequenced task runner. Drain all the
    // scheduled tasks to ensure that the file writing ones have run before
    // checking if they exist.
    base::ThreadPoolInstance::Get()->FlushForTesting();
    return base::PathExists(log_path_);
  }

 protected:
  std::unique_ptr<FileNetLogObserver> logger_;
  base::ScopedTempDir temp_dir_;
  base::ScopedTempDir scratch_dir_;  // used for bounded + preexisting
  base::FilePath log_path_;
};

// Used for tests that are exclusive to the bounded mode of FileNetLogObserver.
class FileNetLogObserverBoundedTest : public ::testing::Test,
                                      public WithTaskEnvironment {
 public:
  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    log_path_ = temp_dir_.GetPath().AppendASCII("net-log.json");
  }

  void TearDown() override {
    logger_.reset();
    // FileNetLogObserver destructor might post to message loop.
    RunUntilIdle();
  }

  void CreateAndStartObserving(std::unique_ptr<base::Value::Dict> constants,
                               uint64_t total_file_size,
                               int num_files) {
    logger_ = FileNetLogObserver::CreateBoundedForTests(
        log_path_, total_file_size, num_files, NetLogCaptureMode::kDefault,
        std::move(constants));
    logger_->StartObserving(NetLog::Get());
  }

  // Returns the path for an internally directory created for bounded logs (this
  // needs to be kept in sync with the implementation).
  base::FilePath GetInprogressDirectory() const {
    return log_path_.AddExtension(FILE_PATH_LITERAL(".inprogress"));
  }

  base::FilePath GetEventFilePath(int index) const {
    return GetInprogressDirectory().AppendASCII(
        "event_file_" + base::NumberToString(index) + ".json");
  }

  base::FilePath GetEndNetlogPath() const {
    return GetInprogressDirectory().AppendASCII("end_netlog.json");
  }

  base::FilePath GetConstantsPath() const {
    return GetInprogressDirectory().AppendASCII("constants.json");
  }


 protected:
  std::unique_ptr<FileNetLogObserver> logger_;
  base::FilePath log_path_;

 private:
  base::ScopedTempDir temp_dir_;
};

// Instantiates each FileNetLogObserverTest to use bounded and unbounded modes.
INSTANTIATE_TEST_SUITE_P(All,
                         FileNetLogObserverTest,
                         ::testing::Values(true, false));

// Tests deleting a FileNetLogObserver without first calling StopObserving().
TEST_P(FileNetLogObserverTest, ObserverDestroyedWithoutStopObserving) {
  CreateAndStartObserving(nullptr);

  // Send dummy event
  AddEntries(logger_.get(), 1, kDummyEventSize);

  // The log files should have been started.
  ASSERT_TRUE(LogFileExists());

  logger_.reset();

  // When the logger is re-set without having called StopObserving(), the
  // partially written log files are deleted.
  ASSERT_FALSE(LogFileExists());
}

// Same but with pre-existing file.
TEST_P(FileNetLogObserverTest,
       ObserverDestroyedWithoutStopObservingPreExisting) {
  CreateAndStartObservingPreExisting(nullptr);

  // Send dummy event
  AddEntries(logger_.get(), 1, kDummyEventSize);

  // The log files should have been started.
  ASSERT_TRUE(LogFileExists());

  // Should also have the scratch dir, if bounded. (Can be checked since
  // LogFileExists flushed the thread pool).
  if (IsBounded()) {
    ASSERT_TRUE(base::PathExists(scratch_dir_.GetPath()));
  }

  logger_.reset();

  // Unlike in the non-preexisting case, the output file isn't deleted here,
  // since the process running the observer likely won't have the sandbox
  // permission to do so.
  ASSERT_TRUE(LogFileExists());
  if (IsBounded()) {
    ASSERT_FALSE(base::PathExists(scratch_dir_.GetPath()));
  }
}

// Tests calling StopObserving() with a null closure.
TEST_P(FileNetLogObserverTest, StopObservingNullClosure) {
  CreateAndStartObserving(nullptr);

  // Send dummy event
  AddEntries(logger_.get(), 1, kDummyEventSize);

  // The log files should have been started.
  ASSERT_TRUE(LogFileExists());

  logger_->StopObserving(nullptr, base::OnceClosure());

  logger_.reset();

  // Since the logger was explicitly stopped, its files should still exist.
  ASSERT_TRUE(LogFileExists());
}

// Tests creating a FileNetLogObserver using an invalid (can't be written to)
// path.
TEST_P(FileNetLogObserverTest, InitLogWithInvalidPath) {
  // Use a path to a non-existent directory.
  log_path_ = temp_dir_.GetPath().AppendASCII("bogus").AppendASCII("path");

  CreateAndStartObserving(nullptr);

  // Send dummy event
  AddEntries(logger_.get(), 1, kDummyEventSize);

  // No log files should have been written, as the log writer will not create
  // missing directories.
  ASSERT_FALSE(LogFileExists());

  logger_->StopObserving(nullptr, base::OnceClosure());

  logger_.reset();

  // There should still be no files.
  ASSERT_FALSE(LogFileExists());
}

TEST_P(FileNetLogObserverTest, GeneratesValidJSONWithNoEvents) {
  TestClosure closure;

  CreateAndStartObserving(nullptr);

  logger_->StopObserving(nullptr, closure.closure());

  closure.WaitForResult();

  // Verify the written log.
  ASSERT_OK_AND_ASSIGN(std::unique_ptr<ParsedNetLog> log,
                       ReadNetLogFromDisk(log_path_));
  ASSERT_EQ(0u, log->events->size());
}

TEST_P(FileNetLogObserverTest, GeneratesValidJSONWithOneEvent) {
  TestClosure closure;

  CreateAndStartObserving(nullptr);

  // Send dummy event.
  AddEntries(logger_.get(), 1, kDummyEventSize);

  logger_->StopObserving(nullptr, closure.closure());

  closure.WaitForResult();

  // Verify the written log.
  ASSERT_OK_AND_ASSIGN(std::unique_ptr<ParsedNetLog> log,
                       ReadNetLogFromDisk(log_path_));
  ASSERT_EQ(1u, log->events->size());
}

TEST_P(FileNetLogObserverTest, GeneratesValidJSONWithOneEventPreExisting) {
  TestClosure closure;

  CreateAndStartObservingPreExisting(nullptr);

  // Send dummy event.
  AddEntries(logger_.get(), 1, kDummyEventSize);

  logger_->StopObserving(nullptr, closure.closure());

  closure.WaitForResult();

  // Verify the written log.
  ASSERT_OK_AND_ASSIGN(std::unique_ptr<ParsedNetLog> log,
                       ReadNetLogFromDisk(log_path_));
  ASSERT_EQ(1u, log->events->size());
}

TEST_P(FileNetLogObserverTest,
       GeneratesValidJSONWithNoEventsCreateBoundedFile) {
  TestClosure closure;

  CreateAndStartObservingBoundedFile(kLargeFileSize, nullptr);

  logger_->StopObserving(nullptr, closure.closure());

  closure.WaitForResult();

  // Verify the written log.
  ASSERT_OK_AND_ASSIGN(std::unique_ptr<ParsedNetLog> log,
                       ReadNetLogFromDisk(log_path_));
  ASSERT_EQ(0u, log->events->size());
}

TEST_P(FileNetLogObserverTest,
       GeneratesValidJSONWithOneEventCreateBoundedFile) {
  TestClosure closure;

  CreateAndStartObservingBoundedFile(kLargeFileSize, nullptr);

  // Send dummy event.
  AddEntries(logger_.get(), 1, kDummyEventSize);

  logger_->StopObserving(nullptr, closure.closure());

  closure.WaitForResult();

  // Verify the written log.
  ASSERT_OK_AND_ASSIGN(std::unique_ptr<ParsedNetLog> log,
                       ReadNetLogFromDisk(log_path_));
  ASSERT_EQ(1u, log->events->size());
}

// Sends exactly enough events to the observer to completely fill the file.
TEST_P(FileNetLogObserverTest, BoundedFileFillsFile) {
  const int kTotalFileSize = 10000;
  const int kEventSize = 200;
  const int kFileSize = kTotalFileSize;
  const int kNumEvents = kFileSize / kEventSize;
  TestClosure closure;

  CreateAndStartObservingBoundedFile(kTotalFileSize, nullptr);

  // Send dummy events.
  AddEntries(logger_.get(), kNumEvents, kEventSize);

  logger_->StopObserving(nullptr, closure.closure());

  closure.WaitForResult();

  // Verify the written log.
  ASSERT_OK_AND_ASSIGN(std::unique_ptr<ParsedNetLog> log,
                       ReadNetLogFromDisk(log_path_));
  VerifyEventsInLog(log.get(), kNumEvents, kNumEvents);
}

// Sends twice as many events as will fill the file to the observer
TEST_P(FileNetLogObserverTest, BoundedFileTruncatesEventsAfterLimit) {
  const int kTotalFileSize = 10000;
  const int kEventSize = 200;
  const int kFileSize = kTotalFileSize;
  const int kNumEvents = kFileSize / kEventSize;
  TestClosure closure;

  CreateAndStartObservingBoundedFile(kTotalFileSize, nullptr);

  // Send dummy events.
  AddEntries(logger_.get(), kNumEvents * 2, kEventSize);

  logger_->StopObserving(nullptr, closure.closure());

  closure.WaitForResult();

  // Verify the written log.
  ASSERT_OK_AND_ASSIGN(std::unique_ptr<ParsedNetLog> log,
                       ReadNetLogFromDisk(log_path_));
  VerifyEventsInLog(log.get(), kNumEvents, kNumEvents);
}

TEST_P(FileNetLogObserverTest, PreExistingFileBroken) {
  // Test that pre-existing output file not being successfully open is
  // tolerated.
  ASSERT_TRUE(scratch_dir_.CreateUniqueTempDir());
  base::File file;
  EXPECT_FALSE(file.IsValid());
  if (IsBounded())
    logger_ = FileNetLogObserver::CreateBoundedPreExisting(
        scratch_dir_.GetPath(), std::move(file), kLargeFileSize,
        NetLogCaptureMode::kDefault, nullptr);
  else
    logger_ = FileNetLogObserver::CreateUnboundedPreExisting(
        std::move(file), NetLogCaptureMode::kDefault, nullptr);
  logger_->StartObserving(NetLog::Get());

  // Send dummy event.
  AddEntries(logger_.get(), 1, kDummyEventSize);
  TestClosure closure;
  logger_->StopObserving(nullptr, closure.closure());
  closure.WaitForResult();
}

TEST_P(FileNetLogObserverTest, CustomConstants) {
  TestClosure closure;

  const char kConstantKey[] = "magic";
  const char kConstantString[] = "poney";
  base::Value::Dict constants;
  constants.SetByDottedPath(kConstantKey, kConstantString);

  CreateAndStartObserving(
      std::make_unique<base::Value::Dict>(std::move(constants)));

  logger_->StopObserving(nullptr, closure.closure());

  closure.WaitForResult();

  // Verify the written log.
  ASSERT_OK_AND_ASSIGN(std::unique_ptr<ParsedNetLog> log,
                       ReadNetLogFromDisk(log_path_));

  // Check that custom constant was correctly printed.
  ExpectDictionaryContainsProperty(*log->constants, kConstantKey,
                                   kConstantString);
}

TEST_P(FileNetLogObserverTest, GeneratesValidJSONWithPolledData) {
  TestClosure closure;

  CreateAndStartObserving(nullptr);

  // Create dummy polled data
  const char kDummyPolledDataPath[] = "dummy_path";
  const char kDummyPolledDataString[] = "dummy_info";
  base::Value::Dict dummy_polled_data;
  dummy_polled_data.SetByDottedPath(kDummyPolledDataPath,
                                    kDummyPolledDataString);

  logger_->StopObserving(
      std::make_unique<base::Value>(std::move(dummy_polled_data)),
      closure.closure());

  closure.WaitForResult();

  // Verify the written log.
  ASSERT_OK_AND_ASSIGN(std::unique_ptr<ParsedNetLog> log,
                       ReadNetLogFromDisk(log_path_));
  ASSERT_EQ(0u, log->events->size());

  // Make sure additional information is present and validate it.
  ASSERT_TRUE(log->polled_data);
  ExpectDictionaryContainsProperty(*log->polled_data, kDummyPolledDataPath,
                                   kDummyPolledDataString);
}

// Ensure that the Capture Mode is recorded as a constant in the NetLog.
TEST_P(FileNetLogObserverTest, LogModeRecorded) {
  struct TestCase {
    NetLogCaptureMode capture_mode;
    const char* expected_value;
  } test_cases[] = {// Challenges that result in success results.
                    {NetLogCaptureMode::kEverything, "Everything"},
                    {NetLogCaptureMode::kIncludeSensitive, "IncludeSensitive"},
                    {NetLogCaptureMode::kDefault, "Default"}};

  TestClosure closure;
  for (const auto& test_case : test_cases) {
    CreateAndStartObserving(nullptr, test_case.capture_mode);
    logger_->StopObserving(nullptr, closure.closure());
    closure.WaitForResult();
    ASSERT_OK_AND_ASSIGN(std::unique_ptr<ParsedNetLog> log,
                         ReadNetLogFromDisk(log_path_));
    ExpectDictionaryContainsProperty(*log->constants, "logCaptureMode",
                                     test_case.expected_value);
  }
}

// Adds events concurrently from several different threads. The exact order of
// events seen by this test is non-deterministic.
TEST_P(FileNetLogObserverTest, AddEventsFromMultipleThreads) {
  const size_t kNumThreads = 10;
  std::vector<std::unique_ptr<base::Thread>> threads(kNumThreads);

#if BUILDFLAG(IS_FUCHSIA)
  // TODO(crbug.com/40625862): Diagnosting logging to determine where
  // this test sometimes hangs.
  LOG(ERROR) << "Create and start threads.";
#endif

  // Start all the threads. Waiting for them to start is to hopefuly improve
  // the odds of hitting interesting races once events start being added.
  for (size_t i = 0; i < threads.size(); ++i) {
    threads[i] = std::make_unique<base::Thread>("WorkerThread" +
                                                base::NumberToString(i));
    threads[i]->Start();
    threads[i]->WaitUntilThreadStarted();
  }

#if BUILDFLAG(IS_FUCHSIA)
  LOG(ERROR) << "Create and start observing.";
#endif

  CreateAndStartObserving(nullptr);

  const size_t kNumEventsAddedPerThread = 200;

#if BUILDFLAG(IS_FUCHSIA)
  LOG(ERROR) << "Posting tasks.";
#endif

  // Add events in parallel from all the threads.
  for (size_t i = 0; i < kNumThreads; ++i) {
    threads[i]->task_runner()->PostTask(
        FROM_HERE, base::BindOnce(&AddEntries, base::Unretained(logger_.get()),
                                  kNumEventsAddedPerThread, kDummyEventSize));
  }

#if BUILDFLAG(IS_FUCHSIA)
  LOG(ERROR) << "Joining all threads.";
#endif

  // Join all the threads.
  threads.clear();

#if BUILDFLAG(IS_FUCHSIA)
  LOG(ERROR) << "Stop observing.";
#endif

  // Stop observing.
  TestClosure closure;
  logger_->StopObserving(nullptr, closure.closure());
  closure.WaitForResult();

#if BUILDFLAG(IS_FUCHSIA)
  LOG(ERROR) << "Read log from disk and verify.";
#endif

  // Verify the written log.
  ASSERT_OK_AND_ASSIGN(std::unique_ptr<ParsedNetLog> log,
                       ReadNetLogFromDisk(log_path_));
  // Check that the expected number of events were written to disk.
  EXPECT_EQ(kNumEventsAddedPerThread * kNumThreads, log->events->size());

#if BUILDFLAG(IS_FUCHSIA)
  LOG(ERROR) << "Teardown.";
#endif
}

// Sends enough events to the observer to completely fill one file, but not
// write any events to an additional file. Checks the file bounds.
TEST_F(FileNetLogObserverBoundedTest, EqualToOneFile) {
  // The total size of the events is equal to the size of one file.
  // |kNumEvents| * |kEventSize| = |kTotalFileSize| / |kTotalNumEvents|
  const int kTotalFileSize = 5000;
  const int kNumEvents = 2;
  const int kEventSize = 250;
  TestClosure closure;

  CreateAndStartObserving(nullptr, kTotalFileSize, kTotalNumFiles);

  AddEntries(logger_.get(), kNumEvents, kEventSize);
  logger_->StopObserving(nullptr, closure.closure());

  closure.WaitForResult();

  // Verify the written log.
  ASSERT_OK_AND_ASSIGN(std::unique_ptr<ParsedNetLog> log,
                       ReadNetLogFromDisk(log_path_));
  VerifyEventsInLog(log.get(), kNumEvents, kNumEvents);
}

// Sends enough events to fill one file, and partially fill a second file.
// Checks the file bounds and writing to a new file.
TEST_F(FileNetLogObserverBoundedTest, OneEventOverOneFile) {
  // The total size of the events is greater than the size of one file, and
  // less than the size of two files. The total size of all events except one
  // is equal to the size of one file, so the last event will be the only event
  // in the second file.
  // (|kNumEvents| - 1) * kEventSize = |kTotalFileSize| / |kTotalNumEvents|
  const int kTotalFileSize = 6000;
  const int kNumEvents = 4;
  const int kEventSize = 200;
  TestClosure closure;

  CreateAndStartObserving(nullptr, kTotalFileSize, kTotalNumFiles);

  AddEntries(logger_.get(), kNumEvents, kEventSize);

  logger_->StopObserving(nullptr, closure.closure());

  closure.WaitForResult();

  // Verify the written log.
  ASSERT_OK_AND_ASSIGN(std::unique_ptr<ParsedNetLog> log,
                       ReadNetLogFromDisk(log_path_));
  VerifyEventsInLog(log.get(), kNumEvents, kNumEvents);
}

// Sends enough events to the observer to completely fill two files.
TEST_F(FileNetLogObserverBoundedTest, EqualToTwoFiles) {
  // The total size of the events is equal to the total size of two files.
  // |kNumEvents| * |kEventSize| = 2 * |kTotalFileSize| / |kTotalNumEvents|
  const int kTotalFileSize = 6000;
  const int kNumEvents = 6;
  const int kEventSize = 200;
  TestClosure closure;

  CreateAndStartObserving(nullptr, kTotalFileSize, kTotalNumFiles);

  AddEntries(logger_.get(), kNumEvents, kEventSize);

  logger_->StopObserving(nullptr, closure.closure());

  closure.WaitForResult();

  // Verify the written log.
  ASSERT_OK_AND_ASSIGN(std::unique_ptr<ParsedNetLog> log,
                       ReadNetLogFromDisk(log_path_));
  VerifyEventsInLog(log.get(), kNumEvents, kNumEvents);
}

// Sends exactly enough events to the observer to completely fill all files,
// so that all events fit into the event files and no files need to be
// overwritten.
TEST_F(FileNetLogObserverBoundedTest, FillAllFilesNoOverwriting) {
  // The total size of events is equal to the total size of all files.
  // |kEventSize| * |kNumEvents| = |kTotalFileSize|
  const int kTotalFileSize = 10000;
  const int kEventSize = 200;
  const int kFileSize = kTotalFileSize / kTotalNumFiles;
  const int kNumEvents = kTotalNumFiles * ((kFileSize - 1) / kEventSize + 1);
  TestClosure closure;

  CreateAndStartObserving(nullptr, kTotalFileSize, kTotalNumFiles);

  AddEntries(logger_.get(), kNumEvents, kEventSize);

  logger_->StopObserving(nullptr, closure.closure());

  closure.WaitForResult();

  // Verify the written log.
  ASSERT_OK_AND_ASSIGN(std::unique_ptr<ParsedNetLog> log,
                       ReadNetLogFromDisk(log_path_));
  VerifyEventsInLog(log.get(), kNumEvents, kNumEvents);
}

// Sends more events to the observer than will fill the WriteQueue, forcing the
// queue to drop an event. Checks that the queue drops the oldest event.
TEST_F(FileNetLogObserverBoundedTest, DropOldEventsFromWriteQueue) {
  // The total size of events is greater than the WriteQueue's memory limit, so
  // the oldest event must be dropped from the queue and not written to any
  // file.
  // |kNumEvents| * |kEventSize| > |kTotalFileSize| * 2
  const int kTotalFileSize = 1000;
  const int kNumEvents = 11;
  const int kEventSize = 200;
  const int kFileSize = kTotalFileSize / kTotalNumFiles;
  TestClosure closure;

  CreateAndStartObserving(nullptr, kTotalFileSize, kTotalNumFiles);

  AddEntries(logger_.get(), kNumEvents, kEventSize);

  logger_->StopObserving(nullptr, closure.closure());

  closure.WaitForResult();

  // Verify the written log.
  ASSERT_OK_AND_ASSIGN(std::unique_ptr<ParsedNetLog> log,
                       ReadNetLogFromDisk(log_path_));
  VerifyEventsInLog(
      log.get(), kNumEvents,
      static_cast<size_t>(kTotalNumFiles * ((kFileSize - 1) / kEventSize + 1)));
}

// Sends twice as many events as will fill all files to the observer, so that
// all of the event files will be filled twice, and every file will be
// overwritten.
TEST_F(FileNetLogObserverBoundedTest, OverwriteAllFiles) {
  // The total size of the events is much greater than twice the number of
  // events that can fit in the event files, to make sure that the extra events
  // are written to a file, not just dropped from the queue.
  // |kNumEvents| * |kEventSize| >= 2 * |kTotalFileSize|
  const int kTotalFileSize = 6000;
  const int kNumEvents = 60;
  const int kEventSize = 200;
  const int kFileSize = kTotalFileSize / kTotalNumFiles;
  TestClosure closure;

  CreateAndStartObserving(nullptr, kTotalFileSize, kTotalNumFiles);

  AddEntries(logger_.get(), kNumEvents, kEventSize);

  logger_->StopObserving(nullptr, closure.closure());

  closure.WaitForResult();

  // Check that the minimum number of events that should fit in event files
  // have been written to all files.
  int events_per_file = (kFileSize - 1) / kEventSize + 1;
  int events_in_last_file = (kNumEvents - 1) % events_per_file + 1;

  // Indicates the total number of events that should be written to all files.
  int num_events_in_files =
      (kTotalNumFiles - 1) * events_per_file + events_in_last_file;

  // Verify the written log.
  ASSERT_OK_AND_ASSIGN(std::unique_ptr<ParsedNetLog> log,
                       ReadNetLogFromDisk(log_path_));
  VerifyEventsInLog(log.get(), kNumEvents,
                    static_cast<size_t>(num_events_in_files));
}

// Sends enough events to the observer to fill all event files, plus overwrite
// some files, without overwriting all of them. Checks that the FileWriter
// overwrites the file with the oldest events.
TEST_F(FileNetLogObserverBoundedTest, PartiallyOverwriteFiles) {
  // The number of events sent to the observer is greater than the number of
  // events that can fit into the event files, but the events can fit in less
  // than twice the number of event files, so not every file will need to be
  // overwritten.
  // |kTotalFileSize| < |kNumEvents| * |kEventSize|
  // |kNumEvents| * |kEventSize| <= (2 * |kTotalNumFiles| - 1) * |kFileSize|
  const int kTotalFileSize = 6000;
  const int kNumEvents = 50;
  const int kEventSize = 200;
  const int kFileSize = kTotalFileSize / kTotalNumFiles;
  TestClosure closure;

  CreateAndStartObserving(nullptr, kTotalFileSize, kTotalNumFiles);

  AddEntries(logger_.get(), kNumEvents, kEventSize);

  logger_->StopObserving(nullptr, closure.closure());

  closure.WaitForResult();

  // Check that the minimum number of events that should fit in event files
  // have been written to a file.
  int events_per_file = (kFileSize - 1) / kEventSize + 1;
  int events_in_last_file = kNumEvents % events_per_file;
  if (!events_in_last_file)
    events_in_last_file = events_per_file;
  int num_events_in_files =
      (kTotalNumFiles - 1) * events_per_file + events_in_last_file;

  // Verify the written log.
  ASSERT_OK_AND_ASSIGN(std::unique_ptr<ParsedNetLog> log,
                       ReadNetLogFromDisk(log_path_));
  VerifyEventsInLog(log.get(), kNumEvents,
                    static_cast<size_t>(num_events_in_files));
}

// Start logging in bounded mode. Create directories in places where the logger
// expects to create files, in order to cause that file creation to fail.
//
//   constants.json      -- succeess
//   event_file_0.json   -- fails to open
//   end_netlog.json     -- fails to open
TEST_F(FileNetLogObserverBoundedTest, SomeFilesFailToOpen) {
  // The total size of events is equal to the total size of all files.
  // |kEventSize| * |kNumEvents| = |kTotalFileSize|
  const int kTotalFileSize = 10000;
  const int kEventSize = 200;
  const int kFileSize = kTotalFileSize / kTotalNumFiles;
  const int kNumEvents = kTotalNumFiles * ((kFileSize - 1) / kEventSize + 1);
  TestClosure closure;

  // Create directories as a means to block files from being created by logger.
  EXPECT_TRUE(base::CreateDirectory(GetEventFilePath(0)));
  EXPECT_TRUE(base::CreateDirectory(GetEndNetlogPath()));

  CreateAndStartObserving(nullptr, kTotalFileSize, kTotalNumFiles);

  AddEntries(logger_.get(), kNumEvents, kEventSize);

  logger_->StopObserving(nullptr, closure.closure());

  closure.WaitForResult();

  // The written log is invalid (and hence can't be parsed). It is just the
  // constants.
  std::string log_contents;
  ASSERT_TRUE(base::ReadFileToString(log_path_, &log_contents));
  // TODO(eroman): Verify the partially written log file?

  // Even though FileNetLogObserver didn't create the directory itself, it will
  // unconditionally delete it. The name should be uncommon enough for this be
  // to reasonable.
  EXPECT_FALSE(base::PathExists(GetInprogressDirectory()));
}

// Start logging in bounded mode. Create a file at the path where the logger
// expects to create its inprogress directory to store event files. This will
// cause logging to completely break. open it.
TEST_F(FileNetLogObserverBoundedTest, InprogressDirectoryBlocked) {
  // The total size of events is equal to the total size of all files.
  // |kEventSize| * |kNumEvents| = |kTotalFileSize|
  const int kTotalFileSize = 10000;
  const int kEventSize = 200;
  const int kFileSize = kTotalFileSize / kTotalNumFiles;
  const int kNumEvents = kTotalNumFiles * ((kFileSize - 1) / kEventSize + 1);
  TestClosure closure;

  // By creating a file where a directory should be, it will not be possible to
  // write any event files.
  EXPECT_TRUE(base::WriteFile(GetInprogressDirectory(), "x"));

  CreateAndStartObserving(nullptr, kTotalFileSize, kTotalNumFiles);

  AddEntries(logger_.get(), kNumEvents, kEventSize);

  logger_->StopObserving(nullptr, closure.closure());

  closure.WaitForResult();

  // There will be a log file at the final output, however it will be empty
  // since nothing was written to the .inprogress directory.
  std::string log_contents;
  ASSERT_TRUE(base::ReadFileToString(log_path_, &log_contents));
  EXPECT_EQ("", log_contents);

  // FileNetLogObserver unconditionally deletes the inprogress path (even though
  // it didn't actually create this file and it was a file instead of a
  // directory).
  // TODO(eroman): Should it only delete if it is a file?
  EXPECT_FALSE(base::PathExists(GetInprogressDirectory()));
}

// Start logging in bounded mode. Create a file with the same name as the 0th
// events file. This will prevent any events from being written.
TEST_F(FileNetLogObserverBoundedTest, BlockEventsFile0) {
  // The total size of events is equal to the total size of all files.
  // |kEventSize| * |kNumEvents| = |kTotalFileSize|
  const int kTotalFileSize = 10000;
  const int kEventSize = 200;
  const int kFileSize = kTotalFileSize / kTotalNumFiles;
  const int kNumEvents = kTotalNumFiles * ((kFileSize - 1) / kEventSize + 1);
  TestClosure closure;

  // Block the 0th events file.
  EXPECT_TRUE(base::CreateDirectory(GetEventFilePath(0)));

  CreateAndStartObserving(nullptr, kTotalFileSize, kTotalNumFiles);

  AddEntries(logger_.get(), kNumEvents, kEventSize);

  logger_->StopObserving(nullptr, closure.closure());

  closure.WaitForResult();

  // Verify the written log.
  ASSERT_OK_AND_ASSIGN(std::unique_ptr<ParsedNetLog> log,
                       ReadNetLogFromDisk(log_path_));
  ASSERT_EQ(0u, log->events->size());
}

// Make sure that when using bounded mode with a pre-existing output file,
// a separate in-progress directory can be specified.
TEST_F(FileNetLogObserverBoundedTest, PreExistingUsesSpecifiedDir) {
  base::ScopedTempDir scratch_dir;
  ASSERT_TRUE(scratch_dir.CreateUniqueTempDir());

  base::File file(log_path_, base::File::FLAG_CREATE | base::File::FLAG_WRITE);
  ASSERT_TRUE(file.IsValid());

  // Stick in some nonsense to make sure the file gets cleared properly
  file.Write(0, "not json", 8);

  logger_ = FileNetLogObserver::CreateBoundedPreExisting(
      scratch_dir.GetPath(), std::move(file), kLargeFileSize,
      NetLogCaptureMode::kDefault, nullptr);
  logger_->StartObserving(NetLog::Get());

  base::ThreadPoolInstance::Get()->FlushForTesting();
  EXPECT_TRUE(base::PathExists(log_path_));
  EXPECT_TRUE(
      base::PathExists(scratch_dir.GetPath().AppendASCII("constants.json")));
  EXPECT_FALSE(base::PathExists(GetInprogressDirectory()));

  TestClosure closure;
  logger_->StopObserving(nullptr, closure.closure());
  closure.WaitForResult();

  // Now the scratch dir should be gone, too.
  EXPECT_FALSE(base::PathExists(scratch_dir.GetPath()));
  EXPECT_FALSE(base::PathExists(GetInprogressDirectory()));
}

// Creates a bounded log with a very large total size and verifies that events
// are not dropped. This is a regression test for https://crbug.com/959929 in
// which the WriteQueue size was calculated by the possibly overflowed
// expression |total_file_size * 2|.
TEST_F(FileNetLogObserverBoundedTest, LargeWriteQueueSize) {
  TestClosure closure;

  // This is a large value such that multiplying it by 2 will overflow to a much
  // smaller value (5).
  uint64_t total_file_size = 0x8000000000000005;

  CreateAndStartObserving(nullptr, total_file_size, kTotalNumFiles);

  // Send 3 dummy events. This isn't a lot of data, however if WriteQueue was
  // initialized using the overflowed value of |total_file_size * 2| (which is
  // 5), then the effective limit would prevent any events from being written.
  AddEntries(logger_.get(), 3, kDummyEventSize);

  logger_->StopObserving(nullptr, closure.closure());

  closure.WaitForResult();

  // Verify the written log.
  ASSERT_OK_AND_ASSIGN(std::unique_ptr<ParsedNetLog> log,
                       ReadNetLogFromDisk(log_path_));
  ASSERT_EQ(3u, log->events->size());
}

void AddEntriesViaNetLog(NetLog* net_log, int num_entries) {
  for (int i = 0; i < num_entries; i++) {
    net_log->AddGlobalEntry(NetLogEventType::PAC_JAVASCRIPT_ERROR);
  }
}

TEST_P(FileNetLogObserverTest, AddEventsFromMultipleThreadsWithStopObserving) {
  const size_t kNumThreads = 10;
  std::vector<std::unique_ptr<base::Thread>> threads(kNumThreads);
  // Start all the threads. Waiting for them to start is to hopefully improve
  // the odds of hitting interesting races once events start being added.
  for (size_t i = 0; i < threads.size(); ++i) {
    threads[i] = std::make_unique<base::Thread>("WorkerThread" +
                                                base::NumberToString(i));
    threads[i]->Start();
    threads[i]->WaitUntilThreadStarted();
  }

  CreateAndStartObserving(nullptr);

  const size_t kNumEventsAddedPerThread = 200;

  // Add events in parallel from all the threads.
  for (size_t i = 0; i < kNumThreads; ++i) {
    threads[i]->task_runner()->PostTask(
        FROM_HERE,
        base::BindOnce(&AddEntriesViaNetLog, base::Unretained(NetLog::Get()),
                       kNumEventsAddedPerThread));
  }

  // Stop observing.
  TestClosure closure;
  logger_->StopObserving(nullptr, closure.closure());
  closure.WaitForResult();

  // Join all the threads.
  threads.clear();

  ASSERT_TRUE(LogFileExists());
}

TEST_P(FileNetLogObserverTest,
       AddEventsFromMultipleThreadsWithoutStopObserving) {
  const size_t kNumThreads = 10;
  std::vector<std::unique_ptr<base::Thread>> threads(kNumThreads);
  // Start all the threads. Waiting for them to start is to hopefully improve
  // the odds of hitting interesting races once events start being added.
  for (size_t i = 0; i < threads.size(); ++i) {
    threads[i] = std::make_unique<base::Thread>("WorkerThread" +
                                                base::NumberToString(i));
    threads[i]->Start();
    threads[i]->WaitUntilThreadStarted();
  }

  CreateAndStartObserving(nullptr);

  const size_t kNumEventsAddedPerThread = 200;

  // Add events in parallel from all the threads.
  for (size_t i = 0; i < kNumThreads; ++i) {
    threads[i]->task_runner()->PostTask(
        FROM_HERE,
        base::BindOnce(&AddEntriesViaNetLog, base::Unretained(NetLog::Get()),
                       kNumEventsAddedPerThread));
  }

  // Destroy logger.
  logger_.reset();

  // Join all the threads.
  threads.clear();

  // The log file doesn't exist since StopObserving() was not called.
  ASSERT_FALSE(LogFileExists());
}

}  // namespace

}  // namespace net
