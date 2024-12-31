// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_COLLABORATION_MODEL_IOS_COLLABORATION_FLOW_CONFIGURATION_H_
#define IOS_CHROME_BROWSER_COLLABORATION_MODEL_IOS_COLLABORATION_FLOW_CONFIGURATION_H_

#import <UIKit/UIKit.h>

#import "components/saved_tab_groups/public/types.h"

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
  explicit CollaborationFlowConfigurationShareOrManage();
  ~CollaborationFlowConfigurationShareOrManage() override;

  // CollaborationFlowConfiguration.
  Type type() const final;
};

// This class is the configuration for a join flow.
class CollaborationFlowConfigurationJoin final
    : public CollaborationFlowConfiguration {
 public:
  static constexpr Type kType = Type::kJoin;

  // Constructs a new CollaborationFlowConfigurationJoin object.
  explicit CollaborationFlowConfigurationJoin();
  ~CollaborationFlowConfigurationJoin() override;

  // CollaborationFlowConfiguration.
  Type type() const final;
};

}  // namespace collaboration

#endif  // IOS_CHROME_BROWSER_COLLABORATION_MODEL_IOS_COLLABORATION_FLOW_CONFIGURATION_H_
