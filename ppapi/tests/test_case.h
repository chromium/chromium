// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_TESTS_TEST_CASE_H_
#define PPAPI_TESTS_TEST_CASE_H_

#include <stdint.h>

#include <cmath>
#include <limits>
#include <map>
#include <set>
#include <sstream>
#include <string>

#include "ppapi/c/pp_resource.h"
#include "ppapi/c/pp_time.h"
#include "ppapi/c/private/ppb_testing_private.h"
#include "ppapi/cpp/message_loop.h"
#include "ppapi/cpp/view.h"
#include "ppapi/tests/test_utils.h"
#include "ppapi/tests/testing_instance.h"

#if (defined __native_client__)
#include "ppapi/cpp/var.h"
#else
#include "ppapi/cpp/private/var_private.h"
#endif

class TestingInstance;

namespace pp {
namespace deprecated {
class ScriptableObject;
}
}

// Individual classes of tests derive from this generic test case.
class TestCase {
 public:
  explicit TestCase(TestingInstance* instance);
  virtual ~TestCase();

  // Optionally override to do testcase specific initialization.
  // Default implementation just returns true.
  virtual bool Init();

  // Override to implement the test case. It will be called after the plugin is
  // first displayed, passing a string. If the string is empty, RunTests should
  // run all tests for this test case. Otherwise, it must be a comma-delimited
  // list of test names, possibly prefixed. E.g.:
  //   "Foo_GoodTest,DISABLED_Foo_BadTest,Foo_OtherGoodTest"
  // All listed tests which are not prefixed will be run.
  //
  // This should generally be implemented in a TestCase subclass using the
  // RUN_TEST* macros.
  virtual void RunTests(const std::string& test_filter) = 0;

  static std::string MakeFailureMessage(const char* file, int line,
                                        const char* cmd);

#if !(defined __native_client__)
  // Returns the scriptable test object for the current test, if any.
  // Internally, this uses CreateTestObject which each test overrides.
  pp::VarPrivate GetTestObject();
  void ResetTestObject() { test_object_ = pp::VarPrivate(); }
#endif

  // A function that is invoked whenever HandleMessage is called on the
  // associated TestingInstance. Default implementation does nothing.  TestCases
  // that want to handle incoming postMessage events should override this
  // method.
  virtual void HandleMessage(const pp::Var& message_data);

  // A function that is invoked whenever DidChangeView is called on the
  // associated TestingInstance. Default implementation does nothing. TestCases
  // that want to handle view changes should override this method.
  virtual void DidChangeView(const pp::View& view);

  // A function that is invoked whenever HandleInputEvent is called on the
  // associated TestingInstance. Default implementation returns false. TestCases
  // that want to handle view changes should override this method.
  virtual bool HandleInputEvent(const pp::InputEvent& event);

  void IgnoreLeakedVar(int64_t id);

  TestingInstance* instance() { return instance_; }

  const PPB_Testing_Private* testing_interface() { return testing_interface_; }

  static void QuitMainMessageLoop(PP_Instance instance);

  const std::map<std::string, bool>& remaining_tests() {
    return remaining_tests_;
  }
  const std::set<std::string>& skipped_tests() {
    return skipped_tests_;
  }

 protected:
#if !(defined __native_client__)
  // Overridden by each test to supply a ScriptableObject corresponding to the
  // test. There can only be one object created for all tests in a given class,
  // so be sure your object is designed to be re-used.
  //
  // This object should be created on the heap. Ownership will be passed to the
  // caller. Return NULL if there is no supported test object (the default).
  virtual pp::deprecated::ScriptableObject* CreateTestObject();
#endif

  // Checks whether the testing interface is available. Returns true if it is,
  // false otherwise. If it is not available, adds a descriptive error. This is
  // for use by tests that require the testing interface.
  bool CheckTestingInterface();

  // Makes sure the test is run over HTTP.
  bool EnsureRunningOverHTTP();

  // Returns true if |filter| only contains a TestCase name, which normally
  // means "run all tests". Some TestCases require special setup for individual
  // tests, and can use this function to decide whether to ignore those tests.
  bool ShouldRunAllTests(const std::string& filter);

  // Return true if the given test name matches the filter. This is true if
  // (a) filter is empty or (b) test_name matches a test name listed in filter
  // exactly.
  bool ShouldRunTest(const std::string& test_name, const std::string& filter);

  // Check for leaked resources and vars at the end of the test. If any exist,
  // return a string with some information about the error. Otherwise, return
  // an empty string.
  //
  // You should pass the error string from the test so far; if it is non-empty,
  // CheckResourcesAndVars will do nothing and return the same string.
  std::string CheckResourcesAndVars(std::string errors);

  PP_TimeTicks NowInTimeTicks();

  // Run the given test method on a background thread and return the result.
  template <class T>
  std::string RunOnThread(std::string(T::*test_to_run)()) {
    if (!testing_interface_) {
      return "Testing blocking callbacks requires the testing interface. In "
             "Chrome, use the --enable-pepper-testing flag.";
    }
    // These tests are only valid if running out-of-process (threading is not
    // supported in-process). For in-process, just consider it a pass.
    if (!testing_interface_->IsOutOfProcess())
      return std::string();
    pp::MessageLoop background_loop(instance_);
    ThreadedTestRunner<T> runner(instance_->pp_instance(),
        static_cast<T*>(this), test_to_run, background_loop);
    RunOnThreadInternal(&ThreadedTestRunner<T>::ThreadFunction, &runner,
                        testing_interface_);
    return runner.result();
  }

  // Pointer to the instance that owns us.
  TestingInstance* instance_;

  // NULL unless InitTestingInterface is called.
  const PPB_Testing_Private* testing_interface_;

  void set_callback_type(CallbackType callback_type) {
    callback_type_ = callback_type;
  }
  CallbackType callback_type() const {
    return callback_type_;
  }

 private:
  template <class T>
  class ThreadedTestRunner {
   public:
    typedef std::string(T::*TestMethodType)();
    ThreadedTestRunner(PP_Instance instance,
                       T* test_case,
                       TestMethodType test_to_run,
                       pp::MessageLoop loop)
        : instance_(instance),
          test_case_(test_case),
          test_to_run_(test_to_run),
          loop_(loop) {
    }
    const std::string& result() { return result_; }
    static void ThreadFunction(void* runner) {
      static_cast<ThreadedTestRunner<T>*>(runner)->Run();
    }

   private:
    void Run() {
      int32_t result = loop_.AttachToCurrentThread();
      static_cast<void>(result); // result is not used in the RELEASE build.
      PP_DCHECK(PP_OK == result);
      result_ = (test_case_->*test_to_run_)();
      // Now give the loop a chance to clean up.
      loop_.PostQuit(true /* should_destroy */);
      loop_.Run();
      // Tell the main thread to quit its nested run loop, now that the test
      // is complete.
      TestCase::QuitMainMessageLoop(instance_);
    }

    std::string result_;
    PP_Instance instance_;
    T* test_case_;
    TestMethodType test_to_run_;
    pp::MessageLoop loop_;
  };

  // The internals for RunOnThread. This allows us to avoid including
  // pp_thread.h in this header file, since it includes system headers like
  // windows.h.
  // RunOnThreadInternal launches a new thread to run |thread_func|, waits
  // for it to complete using RunMessageLoop(), then joins.
  void RunOnThreadInternal(void (*thread_func)(void*),
                           void* thread_param,
                           const PPB_Testing_Private* testing_interface);

  static void DoQuitMainMessageLoop(void* pp_instance, int32_t result);

  // Passed when creating completion callbacks in some tests. This determines
  // what kind of callback we use for the test.
  CallbackType callback_type_;

  // Var ids that should be ignored when checking for leaks on shutdown.
  std::set<int64_t> ignored_leaked_vars_;

  // The tests that were found in test_filter. The bool indicates whether the
  // test should be run (i.e., it will be false if the test name was prefixed in
  // the test_filter string).
  //
  // This is initialized lazily the first time that ShouldRunTest is called.
  std::map<std::string, bool> filter_tests_;
  // Flag indicating whether we have populated filter_tests_ yet.
  bool have_populated_filter_tests_;
  // This is initialized with the contents of filter_tests_. As each test is
  // run, it is removed from remaining_tests_. When RunTests is finished,
  // remaining_tests_ should be empty. Any remaining tests are tests that were
  // listed in the test_filter but didn't match any calls to ShouldRunTest,
  // meaning it was probably a typo. TestingInstance should log this and
  // consider it a failure.
  std::map<std::string, bool> remaining_tests_;

  // If ShouldRunTest is called but the given test name doesn't match anything
  // in the test_filter, the test name will be added here. This allows
  // TestingInstance to detect when not all tests were listed.
  std::set<std::string> skipped_tests_;

#if !(defined __native_client__)
  // Holds the test object, if any was retrieved from CreateTestObject.
  pp::VarPrivate test_object_;
#endif
};

// This class is an implementation detail.
class TestCaseFactory {
 public:
  typedef TestCase* (*Method)(TestingInstance* instance);

  TestCaseFactory(const char* name, Method method)
      : next_(head_),
        name_(name),
        method_(method) {
    head_ = this;
  }

 private:
  friend class TestingInstance;

  TestCaseFactory* next_;
  const char* name_;
  Method method_;

  static TestCaseFactory* head_;
};

namespace internal {

// The internal namespace contains implementation details that are used by
// the ASSERT macros.

// This base class provides a ToString that works for classes that can be
// converted to a string using std::stringstream. Later, we'll do
// specializations for types that we know will work with this approach.
template <class T>
struct StringinatorBase {
  static std::string ToString(const T& value) {
    std::stringstream stream;
    stream << value;
    return stream.str();
  }
 protected:
  // Not implemented, do not use.
  // Note, these are protected because Windows complains if I make these private
  // and then inherit StringinatorBase (even though they're never used).
  StringinatorBase();
  ~StringinatorBase();
};

// This default class template is for types that we don't recognize as
// something we can convert into a string using stringstream. Types that we
// know *can* be turned to a string should have specializations below.
template <class T>
struct Stringinator {
  static std::string ToString(const T& value) {
    return std::string();
  }
 private:
  // Not implemented, do not use.
  Stringinator();
  ~Stringinator();
};

// Define some full specializations for types that can just use stringstream.
#define DEFINE_STRINGINATOR_FOR_TYPE(type) \
  template <>                              \
  struct Stringinator<type> : public StringinatorBase<type> {}
DEFINE_STRINGINATOR_FOR_TYPE(int32_t);
DEFINE_STRINGINATOR_FOR_TYPE(uint32_t);
DEFINE_STRINGINATOR_FOR_TYPE(int64_t);
DEFINE_STRINGINATOR_FOR_TYPE(uint64_t);
DEFINE_STRINGINATOR_FOR_TYPE(float);
DEFINE_STRINGINATOR_FOR_TYPE(double);
DEFINE_STRINGINATOR_FOR_TYPE(bool);
DEFINE_STRINGINATOR_FOR_TYPE(std::string);
#undef DEFINE_STRINGINATOR_FOR_TYPE

template <class T>
std::string ToString(const T& param) {
  return Stringinator<T>::ToString(param);
}

// This overload is necessary to allow enum values (such as those from
// pp_errors.h, including PP_OK) to work. They won't automatically convert to
// an integral type to instantiate the above function template.
inline std::string ToString(int32_t param) {
  return Stringinator<int32_t>::ToString(param);
}

inline std::string ToString(const char* c_string) {
  return std::string(c_string);
}

// This overload deals with pointers.
template <class T>
std::string ToString(const T* ptr) {
  uintptr_t ptr_val = reinterpret_cast<uintptr_t>(ptr);
  std::stringstream stream;
  stream << ptr_val;
  return stream.str();
}

// ComparisonHelper classes wrap the left-hand parameter of a binary comparison
// ASSERT. The correct class gets chosen based on whether or not it's a NULL or
// 0 literal. If it is a NULL/0 literal, we use NullLiteralComparisonHelper.
// For all other parameters, we use ComparisonHelper. There's also a
// specialization of ComparisonHelper for int below (see below for why
// that is.)
//
// ComparisonHelper does two things for the left param:
//  1) Provides all the appropriate CompareXX functions (CompareEQ, etc).
//  2) Provides ToString.
template <class T>
struct ComparisonHelper {
  explicit ComparisonHelper(const T& param) : value(param) {}
  template <class U>
  bool CompareEQ(const U& right) const {
    return value == right;
  }
  template <class U>
  bool CompareNE(const U& right) const {
    return value != right;
  }
  template <class U>
  bool CompareLT(const U& right) const {
    return value < right;
  }
  template <class U>
  bool CompareGT(const U& right) const {
    return value > right;
  }
  template <class U>
  bool CompareLE(const U& right) const {
    return value <= right;
  }
  template <class U>
  bool CompareGE(const U& right) const {
    return value >= right;
  }
  std::string ToString() const {
    return internal::ToString(value);
  }
  const T& value;
};

// Used for NULL or 0.
struct NullLiteralComparisonHelper {
  NullLiteralComparisonHelper() : value(0) {}
  template <class U>
  bool CompareEQ(const U& right) const {
    return 0 == right;
  }
  template <class U>
  bool CompareNE(const U& right) const {
    return 0 != right;
  }
  template <class U>
  bool CompareLT(const U& right) const {
    return 0 < right;
  }
  template <class U>
  bool CompareGT(const U& right) const {
    return 0 > right;
  }
  template <class U>
  bool CompareLE(const U& right) const {
    return 0 <= right;
  }
  template <class U>
  bool CompareGE(const U& right) const {
    return 0 >= right;
  }
  std::string ToString() const {
    return std::string("0");
  }
  const int value;
};

// This class makes it safe to use an integer literal (like 5, or 123) when
// comparing with an unsigned. For example:
// ASSERT_EQ(1, some_vector.size());
// We do a lot of those comparisons, so this makes it easy to get it right
// (rather than forcing assertions to use unsigned literals like 5u or 123u).
//
// This is slightly risky; we're static_casting an int to whatever's on the
// right. If the left value is negative and the right hand side is a large
// unsigned value, it's possible that the comparison will succeed when maybe
// it shouldn't have.
// TODO(dmichael): It should be possible to fix this and upgrade int32_t and
//                 uint32_t to int64_t for the comparison, and make any unsafe
//                 comparisons into compile errors.
template <>
struct ComparisonHelper<int> {
  explicit ComparisonHelper(int param) : value(param) {}
  template <class U>
  bool CompareEQ(const U& right) const {
    return static_cast<U>(value) == right;
  }
  template <class U>
  bool CompareNE(const U& right) const {
    return static_cast<U>(value) != right;
  }
  template <class U>
  bool CompareLT(const U& right) const {
    return static_cast<U>(value) < right;
  }
  template <class U>
  bool CompareGT(const U& right) const {
    return static_cast<U>(value) > right;
  }
  template <class U>
  bool CompareLE(const U& right) const {
    return static_cast<U>(value) <= right;
  }
  template <class U>
  bool CompareGE(const U& right) const {
    return static_cast<U>(value) >= right;
  }
  std::string ToString() const {
    return internal::ToString(value);
  }
  const int value;
 private:
};

// The default is for the case there the parameter is *not* a NULL or 0 literal.
template <bool is_null_literal>
struct ParameterWrapper {
  template <class T>
  static ComparisonHelper<T> WrapValue(const T& value) {
    return ComparisonHelper<T>(value);
  }
  // This overload is so that we can deal with values from anonymous enums,
  // like the one in pp_errors.h. The function template above won't be
  // considered a match by the compiler.
  static ComparisonHelper<int> WrapValue(int value) {
    return ComparisonHelper<int>(value);
  }
};

// The parameter to WrapValue *is* a NULL or 0 literal.
template <>
struct ParameterWrapper<true> {
  // We just use "..." and ignore the parameter. This sidesteps some problems we
  // would run in to (not all compilers have the same set of constraints).
  // - We can't use a pointer type, because int and enums won't convert.
  // - We can't use an integral type, because pointers won't convert.
  // - We can't overload, because it will sometimes be ambiguous.
  // - We can't templatize and deduce the parameter. Some compilers will deduce
  //   int for NULL, and then refuse to convert NULL to an int.
  //
  // We know in this case that the value is 0, so there's no need to capture the
  // value. We also know it's a fundamental type, so it's safe to pass to "...".
  // (It's illegal to pass non-POD types to ...).
  static NullLiteralComparisonHelper WrapValue(...) {
    return NullLiteralComparisonHelper();
  }
};

// IS_NULL_LITERAL(type) is a little template metaprogramming for determining
// if a type is a null or zero literal (NULL or 0 or a constant that evaluates
// to one of those).
// The idea is that for NULL or 0, any pointer type is always a better match
// than "...". But no other pointer types or literals should convert
// automatically to InternalDummyClass.
struct InternalDummyClass {};
char TestNullLiteral(const InternalDummyClass*);
struct BiggerThanChar { char dummy[2]; };
BiggerThanChar TestNullLiteral(...);
// If the compiler chooses the overload of TestNullLiteral which returns char,
// then we know the value converts automatically to InternalDummyClass*, which
// should only be true of NULL and 0 constants.
#define IS_NULL_LITERAL(a) sizeof(internal::TestNullLiteral(a)) == sizeof(char)

template <class T, class U>
static std::string MakeBinaryComparisonFailureMessage(
    const char* comparator,
    const T& left,
    const U& right,
    const char* left_precompiler_string,
    const char* right_precompiler_string,
    const char* file_name,
    int line_number) {
  std::string error_msg =
      std::string("Failed ASSERT_") + comparator + "(" +
      left_precompiler_string + ", " + right_precompiler_string + ")";
  std::string left_string(left.ToString());
  std::string right_string(ToString(right));
  if (!left_string.empty())
    error_msg += " Left: (" + left_string + ")";

  if (!right_string.empty())
    error_msg += " Right: (" + right_string + ")";

  return TestCase::MakeFailureMessage(file_name, line_number,
                                      error_msg.c_str());
}

// The Comparison function templates allow us to pass the parameter for
// ASSERT macros below and have them be evaluated only once. This is important
// for cases where the parameter might be an expression with side-effects, like
// a function call.
#define DEFINE_COMPARE_FUNCTION(comparator_name) \
template <class T, class U> \
std::string Compare ## comparator_name ( \
    const T& left, \
    const U& right, \
    const char* left_precompiler_string, \
    const char* right_precompiler_string, \
    const char* file_name, \
    int line_num) { \
  if (!(left.Compare##comparator_name(right))) { \
    return MakeBinaryComparisonFailureMessage(#comparator_name, \
                                              left, \
                                              right, \
                                              left_precompiler_string, \
                                              right_precompiler_string, \
                                              file_name, \
                                              line_num); \
  } \
  return std::string(); \
}
DEFINE_COMPARE_FUNCTION(EQ)
DEFINE_COMPARE_FUNCTION(NE)
DEFINE_COMPARE_FUNCTION(LT)
DEFINE_COMPARE_FUNCTION(LE)
DEFINE_COMPARE_FUNCTION(GT)
DEFINE_COMPARE_FUNCTION(GE)
#undef DEFINE_COMPARE_FUNCTION
inline std::string CompareDoubleEq(ComparisonHelper<double> left,
                                   double right,
                                   const char* left_precompiler_string,
                                   const char* right_precompiler_string,
                                   const char* file_name,
                                   int linu_num) {
  if (!(std::fabs(left.value - right) <=
        std::numeric_limits<double>::epsilon())) {
    return MakeBinaryComparisonFailureMessage(
        "~=", left, right, left_precompiler_string, right_precompiler_string,
        __FILE__, __LINE__);
  }
  return std::string();
}

}  // namespace internal

// Use the REGISTER_TEST_CASE macro in your TestCase implementation file to
// register your TestCase.  If your test is named TestFoo, then add the
// following to test_foo.cc:
//
//   REGISTER_TEST_CASE(Foo);
//
// This will cause your test to be included in the set of known tests.
//
#define REGISTER_TEST_CASE(name)                                            \
  static TestCase* Test##name##_FactoryMethod(TestingInstance* instance) {  \
    return new Test##name(instance);                                        \
  }                                                                         \
  static TestCaseFactory g_Test##name_factory(                              \
    #name, &Test##name##_FactoryMethod                                      \
  )

// Helper macro for calling functions implementing specific tests in the
// RunTest function. This assumes the function name is TestFoo where Foo is the
// test |name|.
#define RUN_TEST(name, test_filter) \
  if (ShouldRunTest(#name, test_filter)) { \
    set_callback_type(PP_OPTIONAL); \
    PP_TimeTicks start_time(NowInTimeTicks()); \
    instance_->LogTest(#name, \
                       CheckResourcesAndVars(Test##name()), \
                       start_time); \
  }

// Like RUN_TEST above but forces functions taking callbacks to complete
// asynchronously on success or error.
#define RUN_TEST_FORCEASYNC(name, test_filter) \
  if (ShouldRunTest(#name, test_filter)) { \
    set_callback_type(PP_REQUIRED); \
    PP_TimeTicks start_time(NowInTimeTicks()); \
    instance_->LogTest(#name"ForceAsync", \
                       CheckResourcesAndVars(Test##name()), \
                       start_time); \
  }

#define RUN_TEST_BLOCKING(test_case, name, test_filter) \
  if (ShouldRunTest(#name, test_filter)) { \
    set_callback_type(PP_BLOCKING); \
    PP_TimeTicks start_time(NowInTimeTicks()); \
    instance_->LogTest( \
        #name"Blocking", \
        CheckResourcesAndVars(RunOnThread(&test_case::Test##name)), \
        start_time); \
  }

#define RUN_TEST_BACKGROUND(test_case, name, test_filter) \
  if (ShouldRunTest(#name, test_filter)) { \
    PP_TimeTicks start_time(NowInTimeTicks()); \
    instance_->LogTest( \
        #name"Background", \
        CheckResourcesAndVars(RunOnThread(&test_case::Test##name)), \
        start_time); \
  }

#define RUN_TEST_FORCEASYNC_AND_NOT(name, test_filter) \
  do { \
    RUN_TEST_FORCEASYNC(name, test_filter); \
    RUN_TEST(name, test_filter); \
  } while (false)

// Run a test with all possible callback types.
#define RUN_CALLBACK_TEST(test_case, name, test_filter) \
  do { \
    RUN_TEST_FORCEASYNC(name, test_filter); \
    RUN_TEST(name, test_filter); \
    RUN_TEST_BLOCKING(test_case, name, test_filter); \
    RUN_TEST_BACKGROUND(test_case, name, test_filter); \
  } while (false)

#define RUN_TEST_WITH_REFERENCE_CHECK(name, test_filter) \
  if (ShouldRunTest(#name, test_filter)) { \
    set_callback_type(PP_OPTIONAL); \
    uint32_t objects = testing_interface_->GetLiveObjectsForInstance( \
        instance_->pp_instance()); \
    std::string error_message = Test##name(); \
    if (error_message.empty() && \
        testing_interface_->GetLiveObjectsForInstance( \
            instance_->pp_instance()) != objects) \
      error_message = MakeFailureMessage(__FILE__, __LINE__, \
          "reference leak check"); \
    PP_TimeTicks start_time(NowInTimeTicks()); \
    instance_->LogTest(#name, \
                       CheckResourcesAndVars(error_message), \
                       start_time); \
  }
// TODO(dmichael): Add CheckResourcesAndVars above when Windows tests pass
//                 cleanly. crbug.com/173503

// Helper macros for checking values in tests, and returning a location
// description of the test fails.
#define ASSERT_TRUE(cmd) \
  do { \
    if (!(cmd)) \
      return MakeFailureMessage(__FILE__, __LINE__, #cmd); \
  } while (false)
#define ASSERT_FALSE(cmd) ASSERT_TRUE(!(cmd))
#define COMPARE_BINARY_INTERNAL(comparison_type, a, b) \
    internal::Compare##comparison_type( \
        internal::ParameterWrapper<IS_NULL_LITERAL(a)>::WrapValue(a), \
        (b), \
        #a, \
        #b, \
        __FILE__, \
        __LINE__)
#define ASSERT_BINARY_INTERNAL(comparison_type, a, b) \
do { \
  std::string internal_assert_result_string = \
      COMPARE_BINARY_INTERNAL(comparison_type, a, b); \
  if (!internal_assert_result_string.empty()) { \
    return internal_assert_result_string; \
  } \
} while(false)
#define ASSERT_EQ(a, b) ASSERT_BINARY_INTERNAL(EQ, a, b)
#define ASSERT_NE(a, b) ASSERT_BINARY_INTERNAL(NE, a, b)
#define ASSERT_LT(a, b) ASSERT_BINARY_INTERNAL(LT, a, b)
#define ASSERT_LE(a, b) ASSERT_BINARY_INTERNAL(LE, a, b)
#define ASSERT_GT(a, b) ASSERT_BINARY_INTERNAL(GT, a, b)
#define ASSERT_GE(a, b) ASSERT_BINARY_INTERNAL(GE, a, b)
#define ASSERT_DOUBLE_EQ(a, b) \
do { \
  std::string internal_assert_result_string = \
      internal::CompareDoubleEq( \
          internal::ParameterWrapper<IS_NULL_LITERAL(a)>::WrapValue(a), \
          (b), \
          #a, \
          #b, \
          __FILE__, \
          __LINE__); \
  if (!internal_assert_result_string.empty()) { \
    return internal_assert_result_string; \
  } \
} while(false)
// Runs |function| as a subtest and asserts that it has passed.
#define ASSERT_SUBTEST_SUCCESS(function) \
  do { \
    std::string result = (function); \
    if (!result.empty()) \
      return TestCase::MakeFailureMessage(__FILE__, __LINE__, result.c_str()); \
  } while (false)

#define PASS() return std::string()

#endif  // PPAPI_TESTS_TEST_CASE_H_
