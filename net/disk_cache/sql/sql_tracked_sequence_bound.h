// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_DISK_CACHE_SQL_SQL_TRACKED_SEQUENCE_BOUND_H_
#define NET_DISK_CACHE_SQL_SQL_TRACKED_SEQUENCE_BOUND_H_

#include <memory>
#include <tuple>
#include <type_traits>
#include <utility>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "base/memory/scoped_refptr.h"
#include "base/notreached.h"
#include "base/task/sequenced_task_runner.h"
#include "base/threading/sequence_bound.h"
#include "net/disk_cache/sql/sql_async_task_manager.h"
#include "net/disk_cache/sql/sql_async_task_token.h"

namespace disk_cache {

// SqlTrackedSequenceBound wraps base::SequenceBound to provide automatic
// asynchronous task tracking.
//
// When AsyncCall() is invoked, this class automatically requests a
// SqlAsyncTaskToken from the provided SqlAsyncTaskManager. The token is
// held alive until the asynchronous task (and its optional Then callback)
// completes. This ensures that the manager can accurately wait for all
// background operations to finish, simplifying test synchronization.
template <typename T>
class SqlTrackedSequenceBound {
 public:
  SqlTrackedSequenceBound() = default;

  template <typename... Args>
  SqlTrackedSequenceBound(scoped_refptr<base::SequencedTaskRunner> task_runner,
                          SqlAsyncTaskManager& async_task_manager,
                          Args&&... args)
      : async_task_manager_(&async_task_manager),
        sequence_bound_(std::move(task_runner), std::forward<Args>(args)...) {
    CHECK(async_task_manager_);
  }

  SqlTrackedSequenceBound(SqlTrackedSequenceBound&&) = default;
  SqlTrackedSequenceBound& operator=(SqlTrackedSequenceBound&&) = default;

  ~SqlTrackedSequenceBound() = default;

  bool is_null() const { return sequence_bound_.is_null(); }
  explicit operator bool() const { return !is_null(); }

  void Reset() { sequence_bound_.Reset(); }

  template <typename R, typename C, typename... Args>
  auto AsyncCall(
      R (C::*method)(Args...),
      const base::Location& location = base::Location::Current()) const {
    CHECK(async_task_manager_);
    return CallProxy<R, decltype(method), std::tuple<>, Args...>(
        sequence_bound_, method, location, async_task_manager_->StartTask());
  }

  template <typename R, typename C, typename... Args>
  auto AsyncCall(
      R (C::*method)(Args...) const,
      const base::Location& location = base::Location::Current()) const {
    CHECK(async_task_manager_);
    return CallProxy<R, decltype(method), std::tuple<>, Args...>(
        sequence_bound_, method, location, async_task_manager_->StartTask());
  }

 private:
  // CallProxy is a helper class returned by AsyncCall(). It collects arguments
  // via WithArgs() and executes the asynchronous call either when Then() is
  // called or when the CallProxy goes out of scope (for void-returning
  // methods).
  //
  // `ArgsTuple` represents the arguments collected so far (built by WithArgs).
  // `MethodArgs` represents the arguments expected by the target method.
  template <typename ReturnType,
            typename MethodRef,
            typename ArgsTuple,
            typename... MethodArgs>
  class CallProxy {
   public:
    CallProxy(const base::SequenceBound<T>& sequence_bound,
              MethodRef method,
              const base::Location& location,
              std::unique_ptr<SqlAsyncTaskToken> token,
              ArgsTuple args = std::tuple<>())
        : sequence_bound_(sequence_bound),
          method_(method),
          location_(location),
          token_(std::move(token)),
          args_(std::move(args)) {
      CHECK(token_);
    }

    CallProxy(const CallProxy&) = delete;
    CallProxy& operator=(const CallProxy&) = delete;

    ~CallProxy() {
      if (!token_) {
        return;
      }
      if constexpr (std::is_void_v<ReturnType>) {
        Execute(base::BindOnce([]() {}));
      } else {
        NOTREACHED()
            << "Then() not invoked for a method that returns a non-void type";
      }
    }

    template <typename... BoundArgs>
    auto WithArgs(BoundArgs&&... args) && {
      static_assert(sizeof...(BoundArgs) == sizeof...(MethodArgs),
                    "Number of arguments provided to WithArgs() does not match "
                    "the method's expected arguments.");
      return CallProxy<ReturnType, MethodRef,
                       std::tuple<std::decay_t<BoundArgs>...>, MethodArgs...>(
          *sequence_bound_, method_, location_, std::move(token_),
          std::make_tuple(std::forward<BoundArgs>(args)...));
    }

    template <typename CallbackType>
    void Then(CallbackType then_callback) && {
      Execute(std::move(then_callback));
    }

   private:
    template <typename CallbackType>
    void Execute(CallbackType then_callback) {
      if constexpr (std::tuple_size_v<ArgsTuple> != sizeof...(MethodArgs)) {
        NOTREACHED() << "Wrong number of arguments provided to WithArgs(). "
                     << "Expected " << sizeof...(MethodArgs) << ", got "
                     << std::tuple_size_v<ArgsTuple> << ".";
      } else {
        // The token is moved into the callback here, ensuring that it remains
        // alive until the asynchronous reply completes.
        auto get_callback = [&]() {
          if constexpr (std::is_void_v<ReturnType>) {
            return base::BindOnce(
                [](std::unique_ptr<SqlAsyncTaskToken>,
                   CallbackType then_callback) {
                  std::move(then_callback).Run();
                },
                std::move(token_), std::move(then_callback));
          } else {
            return base::BindOnce(
                [](std::unique_ptr<SqlAsyncTaskToken>,
                   CallbackType then_callback, ReturnType result) {
                  std::move(then_callback).Run(std::move(result));
                },
                std::move(token_), std::move(then_callback));
          }
        };

        std::apply(
            [&](auto&&... args) {
              if constexpr (sizeof...(MethodArgs) == 0) {
                sequence_bound_->AsyncCall(method_, location_)
                    .Then(get_callback());
              } else {
                sequence_bound_->AsyncCall(method_, location_)
                    .WithArgs(std::forward<decltype(args)>(args)...)
                    .Then(get_callback());
              }
            },
            std::move(args_));
      }
    }

    raw_ref<const base::SequenceBound<T>> sequence_bound_;
    MethodRef method_;
    base::Location location_;
    std::unique_ptr<SqlAsyncTaskToken> token_;
    ArgsTuple args_;
  };

  raw_ptr<SqlAsyncTaskManager> async_task_manager_ = nullptr;
  base::SequenceBound<T> sequence_bound_;
};

}  // namespace disk_cache

#endif  // NET_DISK_CACHE_SQL_SQL_TRACKED_SEQUENCE_BOUND_H_
