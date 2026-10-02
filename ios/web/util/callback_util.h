// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_UTIL_CALLBACK_UTIL_H_
#define IOS_WEB_UTIL_CALLBACK_UTIL_H_

#include <tuple>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"

namespace web {

// Returns a new callback with the same signature as `callback` such that
// calling the returned callback will call `callback` with the same args.
// However, if the callback is destroyed without being called, `callback`
// will be invoked synchronously with `default_args`.
//
// Used mostly to implement EnsureBlockCalled(...) but can be used if the
// block from WebKit has already been wrapped in callback.
//
// TODO(crbug.com/567488505): find a way to merge this with the mojo template
// WrapCallbackWithDefaultInvokeIfNotRun(...) which does the same thing (but
// is in a large target).
template <typename... RunArgs, typename... DefaultArgs>
[[nodiscard]] base::OnceCallback<void(RunArgs...)> EnsureCallbackCalled(
    base::OnceCallback<void(RunArgs...)> callback,
    DefaultArgs&&... default_args) {
  std::pair<base::OnceCallback<void(RunArgs...)>,
            base::OnceCallback<void(RunArgs...)>>
      pair = base::SplitOnceCallback(std::move(callback));

  base::ScopedClosureRunner default_runner(base::BindOnce(
      std::move(pair.second), std::forward<DefaultArgs>(default_args)...));

  return base::BindOnce(
      [](base::ScopedClosureRunner default_runner,
         base::OnceCallback<void(RunArgs...)> inner_callback,
         RunArgs... run_args) {
        std::ignore = default_runner.Release();
        std::move(inner_callback).Run(std::forward<RunArgs>(run_args)...);
      },
      std::move(default_runner), std::move(pair.first));
}

// Returns a new callback with the same signature as `block` such that
// calling the returned callback will call `block` with the same args.
// However, if the callback is destroyed without being called, `block`
// will be invoked synchronously with `default_args`.
//
// Used mostly to wrap WebKit completion handler as WebKit asserts they
// are called (and will terminate the application if the block is never
// called).
//
// If the block has already been converted to a callback, you can use
// EnsureCallbackCalled(...) instead.
//
// The expected pattern is to directly wrap the completion handler when
// received from WebKit with default values indicating e.g. an error and
// then to only manipulate the callback.
template <typename... RunArgs, typename... DefaultArgs>
[[nodiscard]] base::OnceCallback<void(RunArgs...)> EnsureBlockCalled(
    void (^block)(RunArgs...),
    DefaultArgs&&... default_args) {
  return EnsureCallbackCalled(base::BindOnce(block),
                              std::forward<DefaultArgs>(default_args)...);
}

}  // namespace web

#endif  // IOS_WEB_UTIL_CALLBACK_UTIL_H_
