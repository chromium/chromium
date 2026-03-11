// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_TABS_MODEL_TAB_HELPER_ATTACHER_H_
#define IOS_CHROME_BROWSER_TABS_MODEL_TAB_HELPER_ATTACHER_H_

#import "base/memory/raw_ref.h"
#import "ios/chrome/browser/tabs/model/tab_helper_filter.h"

class ProfileIOS;
namespace web {
class WebState;
}

// Returns whether the `flag` is set in `mask`.
constexpr bool IsTabHelperFilterMaskSet(TabHelperFilter mask,
                                        TabHelperFilter flag) {
  return (mask & flag) == flag;
}

// A builder class to declaratively attach tab helpers to a WebState.
class TabHelperAttacher {
 public:
  template <typename T>
  class [[nodiscard]] TypedTabHelperAttacher {
   public:
    TypedTabHelperAttacher(bool condition,
                           const raw_ref<web::WebState> web_state,
                           TabHelperAttacher& attacher)
        : condition_(condition), web_state_(web_state), attacher_(attacher) {}

    template <typename... Args>
    void operator()(Args&&... args) {
      if (condition_) {
        T::CreateForWebState(&*web_state_, std::forward<Args>(args)...);
      }
    }

    template <typename... Factories>
    void WithFactory(ProfileIOS* profile) {
      if (condition_) {
        T::CreateForWebState(&*web_state_,
                             Factories::GetForProfile(profile)...);
      }
    }

    template <typename... Functors>
    void With(Functors... functors) {
      if (condition_) {
        T::CreateForWebState(&*web_state_, functors()...);
      }
    }

   private:
    bool condition_;
    const raw_ref<web::WebState> web_state_;
    const raw_ref<TabHelperAttacher> attacher_;
  };

  TabHelperAttacher(web::WebState* web_state, TabHelperFilter filter_flags);
  ~TabHelperAttacher();

  // APIs for usage in `AttachTabHelpers`.

  // Creates a tab helper with all of the provided arguments initialized.
  // Example usage:
  //     TabHelperAttacher attacher;
  //     attacher.Create<TabHelper>(arg1, arg2);
  template <typename T, typename... Args>
  void Create(Args&&... args) {
    TypedTabHelperAttacher<T>(true, web_state_,
                              *this)(std::forward<Args>(args)...);
  }

  // Creates a tab helper with all of the provided arguments initialized if the
  // provided condition is met.
  // Example usage:
  //     TabHelperAttacher attacher;
  //     attacher.CreateWhen<TabHelper>(condition, arg1, arg2);
  template <typename T, typename... Args>
  void CreateWhen(bool condition, Args&&... args) {
    TypedTabHelperAttacher<T>(condition, web_state_,
                              *this)(std::forward<Args>(args)...);
  }

  // Creates a tab helper if the provided condition is met. Requires providing
  // additional specification to initialize deferred arguments.
  // Example usage:
  //     TabHelperAttacher attacher;
  //     attacher.CreateDeferredWhen<TabHelper>(condition)
  //             .With([&](){ return service; });
  //     attacher.CreateDeferredWhen<TabHelper2>(condition)
  //             .WithFactory<TabHelperServiceFactory>(profile);
  template <typename T>
  TypedTabHelperAttacher<T> CreateDeferredWhen(bool condition) {
    return TypedTabHelperAttacher<T>(condition, web_state_, *this);
  }

  // Getters for properties that might be needed for complex conditions.
  ProfileIOS* GetProfile() const { return &*profile_; }
  bool IsOffTheRecord() const { return is_off_the_record_; }
  bool IsForPrerender() const { return for_prerender_; }
  bool IsForLensOverlay() const { return for_lens_overlay_; }
  bool IsForReaderMode() const { return for_reader_mode_; }
  bool IsForStandardNavigation() const {
    return !for_lens_overlay_ && !for_reader_mode_;
  }
  bool IsNotInTabHelperFilter() const {
    return !for_prerender_ && !for_lens_overlay_ && !for_reader_mode_;
  }

 private:
  const raw_ref<web::WebState> web_state_;
  const raw_ref<ProfileIOS> profile_;
  const bool is_off_the_record_;
  const bool for_prerender_;
  const bool for_lens_overlay_;
  const bool for_reader_mode_;
};

#endif  // IOS_CHROME_BROWSER_TABS_MODEL_TAB_HELPER_ATTACHER_H_
