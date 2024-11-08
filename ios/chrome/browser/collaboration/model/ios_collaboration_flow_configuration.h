// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_COLLABORATION_MODEL_IOS_COLLABORATION_FLOW_CONFIGURATION_H_
#define IOS_CHROME_BROWSER_COLLABORATION_MODEL_IOS_COLLABORATION_FLOW_CONFIGURATION_H_

#import <UIKit/UIKit.h>

#import "base/memory/raw_ptr.h"
#import "base/memory/weak_ptr.h"
#import "url/gurl.h"

@class CommandDispatcher;
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

  // Casts the dialog to the given type.
  template <typename T>
  const T& As() const {
    CHECK(type() == T::kType);
    return static_cast<const T&>(*this);
  }

 protected:
  CollaborationFlowConfiguration() = default;
};

// Represents the share flow configuration.
class CollaborationFlowConfigurationShare final
    : public CollaborationFlowConfiguration {
 public:
  static constexpr Type kType = Type::kShare;

  // Constructs a new CollaborationFlowConfigurationShare object.
  explicit CollaborationFlowConfigurationShare(
      ShareKitService* share_kit_service,
      CommandDispatcher* command_dispatcher,
      base::WeakPtr<const TabGroup> tab_group,
      UIViewController* base_view_controller);
  ~CollaborationFlowConfigurationShare() override;

  // Returns the type of the collaboration flow configuration.
  Type type() const final;

  // Returns the ShareKitService associated with the flow configuration.
  raw_ptr<ShareKitService> share_kit_service() const {
    return share_kit_service_;
  }

  // Returns the tab group associated with the flow configuration.
  base::WeakPtr<const TabGroup> tab_group() const { return tab_group_; }

  // Returns the command dispatcher associated with the flow configuration.
  CommandDispatcher* command_dispatcher() const { return command_dispatcher_; }

  // Returns the base view controller associated with the flow configuration.
  UIViewController* base_view_controller() const {
    return base_view_controller_;
  }

 private:
  raw_ptr<ShareKitService> share_kit_service_;
  base::WeakPtr<const TabGroup> tab_group_;
  __weak CommandDispatcher* command_dispatcher_;
  __weak UIViewController* base_view_controller_;
};

// Represents the join flow configuration.
class CollaborationFlowConfigurationJoin final
    : public CollaborationFlowConfiguration {
 public:
  static constexpr Type kType = Type::kJoin;

  // Constructs a new CollaborationFlowConfigurationShare object.
  explicit CollaborationFlowConfigurationJoin(
      ShareKitService* share_kit_service,
      const GURL& url,
      UIViewController* base_view_controller);
  ~CollaborationFlowConfigurationJoin() override;

  // Returns the type of the collaboration flow configuration.
  Type type() const final;

  // Returns the ShareKitService associated with the flow configuration.
  raw_ptr<ShareKitService> share_kit_service() const {
    return share_kit_service_;
  }

  // Returns URL containing the collab ID and the token.
  const GURL& url() const { return url_; }

  // Returns the base view controller associated with the flow configuration.
  UIViewController* base_view_controller() const {
    return base_view_controller_;
  }

 private:
  raw_ptr<ShareKitService> share_kit_service_;
  const GURL url_;
  __weak UIViewController* base_view_controller_;
};

}  // namespace collaboration

#endif  // IOS_CHROME_BROWSER_COLLABORATION_MODEL_IOS_COLLABORATION_FLOW_CONFIGURATION_H_
