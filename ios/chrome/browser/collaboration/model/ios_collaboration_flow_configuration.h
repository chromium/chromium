// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_COLLABORATION_MODEL_IOS_COLLABORATION_FLOW_CONFIGURATION_H_
#define IOS_CHROME_BROWSER_COLLABORATION_MODEL_IOS_COLLABORATION_FLOW_CONFIGURATION_H_

#import <UIKit/UIKit.h>

#import "base/memory/raw_ptr.h"
#import "base/memory/weak_ptr.h"
#import "url/gurl.h"

class Browser;
class ShareKitService;
class TabGroup;

namespace collaboration {

// Represents a generic collaboration flow configuration.
class CollaborationFlowConfiguration {
 public:
  // The type of collaboration flow.
  enum class Type {
    // Share flow.
    kShare,
    // Join flow.
    kJoin,
  };

  // Non-copyable, non-moveable.
  CollaborationFlowConfiguration(const CollaborationFlowConfiguration&) =
      delete;
  CollaborationFlowConfiguration& operator=(
      const CollaborationFlowConfiguration&) = delete;

  virtual ~CollaborationFlowConfiguration() = default;

  // Returns the type of the collaboration flow configuration.
  virtual Type type() const = 0;

  // Returns the ShareKitService associated with the flow configuration.
  raw_ptr<ShareKitService> share_kit_service() const {
    return share_kit_service_;
  }

  // Returns the browser associated with the flow configuration.
  Browser* browser() const { return browser_; }

  // Returns the base view controller associated with the flow configuration.
  UIViewController* base_view_controller() const {
    return base_view_controller_;
  }

  // Casts the dialog to the given type.
  template <typename T>
  const T& As() const {
    CHECK(type() == T::kType);
    return static_cast<const T&>(*this);
  }

 protected:
  CollaborationFlowConfiguration(ShareKitService* share_kit_service,
                                 Browser* browser,
                                 UIViewController* base_view_controller);

 private:
  raw_ptr<ShareKitService> share_kit_service_;
  raw_ptr<Browser> browser_;
  __weak UIViewController* base_view_controller_;
};

// Represents the share flow configuration.
class CollaborationFlowConfigurationShare final
    : public CollaborationFlowConfiguration {
 public:
  static constexpr Type kType = Type::kShare;

  // Constructs a new CollaborationFlowConfigurationShare object.
  explicit CollaborationFlowConfigurationShare(
      ShareKitService* share_kit_service,
      Browser* browser,
      UIViewController* base_view_controller,
      base::WeakPtr<const TabGroup> tab_group);
  ~CollaborationFlowConfigurationShare() override;

  // CollaborationFlowConfiguration.
  Type type() const final;

  // Returns the tab group associated with the flow configuration.
  base::WeakPtr<const TabGroup> tab_group() const { return tab_group_; }

 private:
  base::WeakPtr<const TabGroup> tab_group_;
};

// Represents the join flow configuration.
class CollaborationFlowConfigurationJoin final
    : public CollaborationFlowConfiguration {
 public:
  static constexpr Type kType = Type::kJoin;

  // Constructs a new CollaborationFlowConfigurationShare object.
  explicit CollaborationFlowConfigurationJoin(
      ShareKitService* share_kit_service,
      Browser* browser,
      UIViewController* base_view_controller,
      const GURL& url);
  ~CollaborationFlowConfigurationJoin() override;

  // CollaborationFlowConfiguration.
  Type type() const final;

  // Returns URL containing the collab ID and the token.
  const GURL& url() const { return url_; }

 private:
  const GURL url_;
};

}  // namespace collaboration

#endif  // IOS_CHROME_BROWSER_COLLABORATION_MODEL_IOS_COLLABORATION_FLOW_CONFIGURATION_H_
