// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GOOGLE_APIS_COMMON_TEST_UTIL_H_
#define GOOGLE_APIS_COMMON_TEST_UTIL_H_

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "google_apis/common/api_error_codes.h"
#include "google_apis/common/base_requests.h"
#include "google_apis/common/task_util.h"

class GURL;

namespace base {
class FilePath;
class RunLoop;
class Value;
}  // namespace base

namespace net {
namespace test_server {
class BasicHttpResponse;
class HttpResponse;
struct HttpRequest;
}  // namespace test_server
}  // namespace net

namespace google_apis {
namespace test_util {

// Runs the closure, and then quits the |run_loop|.
void RunAndQuit(base::RunLoop* run_loop, base::OnceClosure closure);

// Returns callback which runs the given |callback| and then quits |run_loop|.
template <typename CallbackType>
CallbackType CreateQuitCallback(base::RunLoop* run_loop,
                                CallbackType callback) {
  return CreateComposedCallback(base::BindOnce(&RunAndQuit, run_loop),
                                std::move(callback));
}

// Removes |prefix| from |input| and stores the result in |output|. Returns
// true if the prefix is removed.
bool RemovePrefix(const std::string& input,
                  const std::string& prefix,
                  std::string* output);

// Returns the absolute path for a test file stored under
// chrome/test/data.
base::FilePath GetTestFilePath(const std::string& relative_path);

// Returns the base URL for communicating with the local test server for
// testing, running at the specified port number.
GURL GetBaseUrlForTesting(int port);

// Writes the |content| to the file at |file_path|. Returns true on success,
// otherwise false.
bool WriteStringToFile(const base::FilePath& file_path,
                       const std::string& content);

// Creates a |size| byte file. The file is filled with random bytes so that
// the test assertions can identify correct portion/position of the file is
// used.
// Returns true on success with the created file's |path| and |data|, otherwise
// false.
bool CreateFileOfSpecifiedSize(const base::FilePath& temp_dir,
                               size_t size,
                               base::FilePath* path,
                               std::string* data);

// Loads a test JSON file as a base::Value, from a test file stored under
// chrome/test/data.
std::unique_ptr<base::Value> LoadJSONFile(const std::string& relative_path);

// Returns a HttpResponse created from the given file path.
std::unique_ptr<net::test_server::BasicHttpResponse> CreateHttpResponseFromFile(
    const base::FilePath& file_path);

// Handles a request for downloading a file. Reads a file from the test
// directory and returns the content. Also, copies the |request| to the memory
// pointed by |out_request|.
// |base_url| must be set to the server's base url.
std::unique_ptr<net::test_server::HttpResponse> HandleDownloadFileRequest(
    const GURL& base_url,
    net::test_server::HttpRequest* out_request,
    const net::test_server::HttpRequest& request);

// Parses a value of Content-Range header, which looks like
// "bytes <start_position>-<end_position>/<length>".
// Returns true on success.
bool ParseContentRangeHeader(const std::string& value,
                             int64_t* start_position,
                             int64_t* end_position,
                             int64_t* length);

// Google API related code work on asynchronous architecture and return the
// results via callbacks. Following code implements a callback to copy such
// results. Here is how to use:
//
//   // Prepare result storage.
//   ResultType1 result1;
//   ResultType2 result2;
//           :
//
//   PerformAsynchronousTask(
//       param1, param2, ...,
//       CreateCopyResultCallback(&result1, &result2, ...));
//   base::RunLoop().RunUntilIdle();  // Run message loop to complete
//                                    // the async task.
//
//   // Hereafter, we can write expectation with results.
//   EXPECT_EQ(expected_result1, result1);
//   EXPECT_EQ(expected_result2, result2);
//                     :
//
// Note: The max arity of the supported function is 4 based on the usage.
namespace internal {
// Following helper templates are to support Chrome's move semantics.
// Their goal is defining helper methods which are similar to:
//   void CopyResultCallback1(T1* out1, T1&& in1)
//   void CopyResultCallback2(T1* out1, T2* out2, T1&& in1, T2&& in2)
//            :
// in C++11.

// Declare if the type is movable or not. Currently limited to scoped_ptr only.
// We can add more types upon the usage.
template <typename T>
struct IsMovable : std::false_type {};
template <typename T, typename D>
struct IsMovable<std::unique_ptr<T, D>> : std::true_type {};

// InType is const T& if |UseConstRef| is true, otherwise |T|.
template <bool UseConstRef, typename T>
struct InTypeHelper {
  typedef const T& InType;
};
template <typename T>
struct InTypeHelper<false, T> {
  typedef T InType;
};

// Simulates the std::move function in C++11. We use pointer here for argument,
// instead of rvalue reference.
template <bool IsMovable, typename T>
struct MoveHelper {
  static const T& Move(const T* in) { return *in; }
};
template <typename T>
struct MoveHelper<true, T> {
  static T Move(T* in) { return std::move(*in); }
};

// Helper to handle Chrome's move semantics correctly.
template <typename T>
struct CopyResultCallbackHelper
    // It is necessary to calculate the exact signature of callbacks we want
    // to create here. In our case, as we use value-parameters for primitive
    // types and movable types in the callback declaration.
    // Thus the incoming type is as follows:
    // 1) If the argument type |T| is class type but doesn't movable,
    //    |InType| is const T&.
    // 2) Otherwise, |T| as is.
    : InTypeHelper<std::is_class<T>::value &&
                       !IsMovable<T>::value,  // UseConstRef
                   T>,
      MoveHelper<IsMovable<T>::value, T> {};

// Copies the |in|'s value to |out|.
template <typename T1>
void CopyResultCallback(T1* out,
                        typename CopyResultCallbackHelper<T1>::InType in) {
  *out = CopyResultCallbackHelper<T1>::Move(&in);
}

// Copies the |in1|'s value to |out1|, and |in2|'s to |out2|.
template <typename T1, typename T2>
void CopyResultCallback(T1* out1,
                        T2* out2,
                        typename CopyResultCallbackHelper<T1>::InType in1,
                        typename CopyResultCallbackHelper<T2>::InType in2) {
  *out1 = CopyResultCallbackHelper<T1>::Move(&in1);
  *out2 = CopyResultCallbackHelper<T2>::Move(&in2);
}

// Copies the |in1|'s value to |out1|, |in2|'s to |out2|, and |in3|'s to |out3|.
template <typename T1, typename T2, typename T3>
void CopyResultCallback(T1* out1,
                        T2* out2,
                        T3* out3,
                        typename CopyResultCallbackHelper<T1>::InType in1,
                        typename CopyResultCallbackHelper<T2>::InType in2,
                        typename CopyResultCallbackHelper<T3>::InType in3) {
  *out1 = CopyResultCallbackHelper<T1>::Move(&in1);
  *out2 = CopyResultCallbackHelper<T2>::Move(&in2);
  *out3 = CopyResultCallbackHelper<T3>::Move(&in3);
}

// Holds the pointers for output. This is introduced for the workaround of
// the arity limitation of Callback.
template <typename T1, typename T2, typename T3, typename T4>
struct OutputParams {
  OutputParams(T1* out1, T2* out2, T3* out3, T4* out4)
      : out1(out1), out2(out2), out3(out3), out4(out4) {}
  raw_ptr<T1> out1;
  raw_ptr<T2> out2;
  raw_ptr<T3> out3;
  raw_ptr<T4> out4;
};

// Copies the |in1|'s value to |output->out1|, |in2|'s to |output->out2|,
// and so on.
template <typename T1, typename T2, typename T3, typename T4>
void CopyResultCallback(const OutputParams<T1, T2, T3, T4>& output,
                        typename CopyResultCallbackHelper<T1>::InType in1,
                        typename CopyResultCallbackHelper<T2>::InType in2,
                        typename CopyResultCallbackHelper<T3>::InType in3,
                        typename CopyResultCallbackHelper<T4>::InType in4) {
  *output.out1 = CopyResultCallbackHelper<T1>::Move(&in1);
  *output.out2 = CopyResultCallbackHelper<T2>::Move(&in2);
  *output.out3 = CopyResultCallbackHelper<T3>::Move(&in3);
  *output.out4 = CopyResultCallbackHelper<T4>::Move(&in4);
}

}  // namespace internal

template <typename T1>
base::OnceCallback<
    void(typename internal::CopyResultCallbackHelper<T1>::InType)>
CreateCopyResultCallback(T1* out1) {
  return base::BindOnce(&internal::CopyResultCallback<T1>, out1);
}

template <typename T1, typename T2>
base::OnceCallback<
    void(typename internal::CopyResultCallbackHelper<T1>::InType,
         typename internal::CopyResultCallbackHelper<T2>::InType)>
CreateCopyResultCallback(T1* out1, T2* out2) {
  return base::BindOnce(&internal::CopyResultCallback<T1, T2>, out1, out2);
}

template <typename T1, typename T2, typename T3>
base::OnceCallback<
    void(typename internal::CopyResultCallbackHelper<T1>::InType,
         typename internal::CopyResultCallbackHelper<T2>::InType,
         typename internal::CopyResultCallbackHelper<T3>::InType)>
CreateCopyResultCallback(T1* out1, T2* out2, T3* out3) {
  return base::BindOnce(&internal::CopyResultCallback<T1, T2, T3>, out1, out2,
                        out3);
}

template <typename T1, typename T2, typename T3, typename T4>
base::OnceCallback<
    void(typename internal::CopyResultCallbackHelper<T1>::InType,
         typename internal::CopyResultCallbackHelper<T2>::InType,
         typename internal::CopyResultCallbackHelper<T3>::InType,
         typename internal::CopyResultCallbackHelper<T4>::InType)>
CreateCopyResultCallback(T1* out1, T2* out2, T3* out3, T4* out4) {
  return base::BindOnce(
      &internal::CopyResultCallback<T1, T2, T3, T4>,
      internal::OutputParams<T1, T2, T3, T4>(out1, out2, out3, out4));
}

typedef std::pair<int64_t, int64_t> ProgressInfo;

// Helper utility for recording the results via ProgressCallback.
void AppendProgressCallbackResult(std::vector<ProgressInfo>* progress_values,
                                  int64_t progress,
                                  int64_t total);

// Helper utility for recording the content via GetContentCallback.
class TestGetContentCallback {
 public:
  TestGetContentCallback();
  TestGetContentCallback(const TestGetContentCallback&) = delete;
  TestGetContentCallback& operator=(const TestGetContentCallback&) = delete;
  ~TestGetContentCallback();

  const GetContentCallback& callback() const { return callback_; }
  const std::vector<std::unique_ptr<std::string>>& data() const {
    return data_;
  }
  std::vector<std::unique_ptr<std::string>>* mutable_data() { return &data_; }
  std::string GetConcatenatedData() const;

 private:
  void OnGetContent(google_apis::ApiErrorCode error,
                    std::unique_ptr<std::string> data,
                    bool first_chunk);

  const GetContentCallback callback_;
  std::vector<std::unique_ptr<std::string>> data_;
};

}  // namespace test_util
}  // namespace google_apis

#endif  // GOOGLE_APIS_COMMON_TEST_UTIL_H_
