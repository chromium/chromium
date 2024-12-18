// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_COLLABORATION_MODEL_IOS_COLLABORATION_FLOW_CONFIGURATION_H_
#define IOS_CHROME_BROWSER_COLLABORATION_MODEL_IOS_COLLABORATION_FLOW_CONFIGURATION_H_

#import <UIKit/UIKit.h>

#import "base/memory/raw_ptr.h"
#import "base/memory/weak_ptr.h"
#import "components/saved_tab_groups/public/types.h"
#import "url/gurl.h"

class TabGroup;

namespace collaboration {

// This class is a generic configuration for a collaboration flow.
class CollaborationFlowConfiguration {
 public:
  // The type of collaboration flow.
  enum class Type {
    // Share or manage flow.
    kShareOrManage,
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

 private:
};

// This class is the configuration for a share or a manage flow.
class CollaborationFlowConfigurationShareOrManage final
    : public CollaborationFlowConfiguration {
 public:
  static constexpr Type kType = Type::kShareOrManage;

  // Constructs a new CollaborationFlowConfigurationShareOrManage object.
  explicit CollaborationFlowConfigurationShareOrManage(
      base::WeakPtr<const TabGroup> tab_group);
  ~CollaborationFlowConfigurationShareOrManage() override;

  // CollaborationFlowConfiguration.
  Type type() const final;

  // Returns the tab group associated with the flow configuration.
  base::WeakPtr<const TabGroup> tab_group() const { return tab_group_; }

 private:
  base::WeakPtr<const TabGroup> tab_group_;
};

// This class is the configuration for a join flow.
class CollaborationFlowConfigurationJoin final
    : public CollaborationFlowConfiguration {
 public:
  static constexpr Type kType = Type::kJoin;

  // Constructs a new CollaborationFlowConfigurationJoin object.
  explicit CollaborationFlowConfigurationJoin(
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
