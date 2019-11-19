This directory supports our branch CI/CQ configuration.

Contents:

* **branches**
  * contains subdirectories that contain shims that set branch-specific
  variables before executing the configuration in the appropriate **milestones**
  subdirectory
  * doesn't contain much actual configuration, so changes should be extremely
  rare
* **milestones.star**
  * module that maps the branches `beta` and `stable` to the milestones
* **milestones**
  * contains subdirectories that contain the versioned configuration for the
  active milestones
  * non-dimension changes should be infrequent
* **trunk**
  * contains the versioned configuration for the current canary/dev builds
  * open to changes
* **vars**
  * contains modules with branch-specific variables; the variables are set by
  the shims in **branches** when executing the configuration in **milestones**

