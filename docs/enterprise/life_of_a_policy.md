# Life of a policy

## Summary

Once a policy is defined and implemented, it will be launched to the public.
After that, any modification or deprecation will require additional cautions and
steps.

This article describes the process and requirements of policy launch,
modification and deprecation.

And if you are not sure, don’t hesitate to contact the [Chrome enterprise team](mailto:chromium-enterprise@chromium.org).

More details about policy development can be found in the [Chromium doc](./add_new_policy.md).

## Launch a new policy

### Before the policy is ready

When adding a new policy, put the platforms it will be supported in the
`future_on` list. This will ensure that the policy is not included in the
auto-generated documentation or templates. It will also exclude the policy from
the Stable and Beta channels unless it's enabled specifically by
[EnableExperimentalPolicies](https://cloud.google.com/docs/chrome-enterprise/policies/?policy=EnableExperimentalPolicies)
policy.

### Review process

Once a policy is implemented and tested, it's time to launch it. Policy launch
process follows the general guidelines of Chrome or Chrome OS launch process.
In short:
* If a policy is part of a big project, please make sure the launch review cover
  it as well.
* If a policy is complicated and is an independent project, please open a
  launch review for it.
* If a policy is trivial, launch review can be skipped.

**Do not use Finch experimentation to manage policy rollout**.
Policy launches require domain-level control for gradual rollout which is not
supported by Finch. However, you could still use Feature to disable a policy
in case of any major issue.

After the feature launch, remove the flag (after 2-3 releases). In some
cases, you may want to wait for the next Long Term Stable (LTS) release before
removing the flag (see [go/chromeos-commercial-lts-chrome](http://go/chromeos-commercial-lts-chrome)).

If you need to launch an emergency kill switch config due to a bug,
please contact your TPM and the release owner (see
[https://chromiumdash.appspot.com/schedule](https://chromiumdash.appspot.com/schedule)).

For more information see [go/chrome-flag-guarding](http://go/chrome-flag-guarding)
(internal doc, Googlers only).

### Trusted testers

Some enterprise features will be tested by a small set of users before the
general launch. Contact the
[Chrome enterprise team](mailto:chromium-enterprise@chromium.org) if needed.

### Flip the bit

Once everything is ready, launch the policy by moving the platforms from
`future_on` to `supported_on` and setting the *since_version* of those platforms
to the milestone for which the launch approval was granted. If the
*since_version* is set to a earlier milestone, you need to merge back all
necessary commits.

## Deprecating a policy

A policy is deprecated when admins should stop using it. This is often because
a new policy has been released and should be used instead.

When deprecating a policy, it's important to ensure that the old policy still
works as expected. If you wish to remove the functionality, you'll need to
change the `supported_on` list.

### Steps

1. Update `YourPolicyName.yaml`, marking the policy as deprecated with the
   `deprecated` field and updating the description to describe when and why the
   policy is deprecated and what admins should be doing instead.
1. Update the policy handling code. This is generally ensuring that if the old
   policy is set, the values are propagated to the new policies if they were
   unset.
1. Notify [Chrome enterprise team](mailto:chromium-enterprise@chromium.org) to
   ensure this deprecation is mentioned in the enterprise release notes.

## Removing support for a policy

Generally speaking, policies shouldn't be removed from Chrome. Although they can
be deprecated fairly easily, removing support for a policy is a much bigger
issue, because admins might be relying on that functionality.

The two main reasons for removing support for a policy are:

1.  It is a policy that was always documented as having a limited lifespan,
    such as an escape hatch policy.

1.  The feature this policy impacts is being removed from Chrome. In such cases,
    policy support should be removed in the same Chrome milestone as the feature
    removal.

If you want to remove support for another reason, please reach out to
[Chrome enterprise team](mailto:chromium-enterprise@chromium.org)
to ensure this is okay. The general preference is to leave policies as
deprecated, but still supported.

If the policy was never launched, `YourPolicyName.yaml` can be deleted and you
may replace the policy name in `policies.yaml` by an empty string.

Otherwise, follow these steps:

1.  Update `YourPolicyName.yaml` to mark the poilcy as no longer supported.

    -   Set `deprecated` to True if the policy skipped past the deprecation
        state.
    -   Update `supported_on` to correctly list the last milestone the policy is
        supported on.
        -   For example, if the impacted feature is being removed in M110, set
            `supported_on` to `X-109`. The policy would have no effect in M110,
            so the last supported milestone is M109.

1.  If the last milestone lies in the future file a bug to clean up the policy
    supporting code as soon as the milestone has been reached. Set its
    NextAction field to a date shortly after the expected branch date for that
    version. Add a comment in the yaml file with the bug number for reference.

1.  Lastly after the last supported version has been branched:

    -   remove all the code that implements the policy behavior after the
        milestone has been reached.
    -   Update the related tests in the under `policy/test/data/pref_mapping` by
        removing all test related to that policy and any resulting empty file.

1.  Notify [Chrome enterprise team](mailto:chromium-enterprise@chromium.org) to
    ensure this removal of support is mentioned in the enterprise release notes.

    -   Ideally, a policy should be deprecated for at least 3 milestones before
        removing support. This gives admins time to migrate away from the
        policy.

## Modifying existing policies

If you are planning to modify an existing policy, notify the
[Chrome enterprise team](mailto:chromium-enterprise@chromium.org) ahead of time.
If necessary, please includes a one-pager to explaining the planned change.

### Add new policy functionality

This usually includes adding a new value to an enum policy, or adding a new
field to a dictionary policy.

The new field needs to be added to the policy schema and mentioned in the
policy description about the versions and platforms it is supported on.

Admins likely will deploy the new field to all their browsers, including the
older ones that don't support it. In this case, the new field should be ignored
properly without disrupting the existing behavior. If not possible, the policy
should be deprecated and replaced by a new one.

### Remove policy functionality

Please follow the same rules of [deprecating](#deprecating-a-policy) or
[removing](#removing-support-for-a-policy) a policy. The main difference is that
instead of updating the `deprecated` and `supported_on` fields, mention
the change in the policy description.

### Any other modification

In general, don't change policy semantics in a way that is incompatible with
previous semantics. Some fields like `default`, `type` or `device_only` should
never be updated after the launch.

However, if a modification is necessary, please update the policy description
to mention the change with the versions and platforms.

### Migration

Unless the policy is `cloud_only` or only supported on ChromeOS, it shouldn't
rely on any cloud policy server for the migration. In other words, all
compatibility and migration code should be contained in the client.

### Presubmit Checks when Modifying Existing Policies

To enforce the above rules concerning policy modification and ensure no
backwards incompatible changes are introduced, presubmit checks
will be performed on every change to
[templates](https://cs.chromium.org/chromium/src/components/policy/resources/templates/policy_definitions/).

The presubmit checks perform the following verifications:

1.  It verifies whether a policy is considered **unreleased** before allowing a
    change. A policy is considered unreleased if **any** of the following
    conditions are true:

    1.  It is an unchanged policy marked as `future: true`.
    2.  All the `supported_versions` of the policy satisfy **any** of the
        following conditions
        1.  The unchanged supported major version is >= the current major
            version stored in the VERSION file at tip of tree. This covers the
            case of a policy that has just recently been added, but has not yet
            been released to a stable branch.
        2.  The changed supported version == unchanged supported version + 1 and
            the changed supported version is equal to the version stored in the
            VERSION file at tip of tree. This check covers the case of
            "unreleasing" a policy after a new stable branch has been cut, but
            before a new stable release has rolled out. Normally such a change
            should eventually be merged into the stable branch before the
            release.
    3. `supported_on` list is empty.

2.  If the policy is considered **unreleased**, all changes to it are allowed.

3.  However if the policy is released then the following verifications
    are performed on the delta between the original policy and the changed
    policy.

    1.  Released policies cannot be removed.
    2.  Released policies cannot have their type changed (e.g. from bool to
        Enum).
    3.  Released policies cannot have the `future: true` flag added to it. This
        flag can only be set on a new policy.
    4.  Released policies can only add additional `supported_on` versions. They
        cannot remove or modify existing values for this field except for the
        special case above for determining if a policy is released. Policy
        support end version (adding "-xx" ) can however be added to the
        supported_on version to specify that a policy will no longer be
        supported going forward (as long as the initial `supported_on` version
        is not changed).
    5.  Released policies cannot be renamed (this is the equivalent of a
        delete + add).
    7.  Released policies with non dict types cannot have their schema changed.
        1.  For enum types, we will allow adding new enum value. Any values that
            are no longer supported should be marked as deprecated.
        2.  For int types, we will allow making the minimum and maximum values
            less restrictive than the existing values.
        3.  For string types, we will allow the removal of the "pattern"
            property to allow the validation to be less restrictive.
        4.  We will allow addition to any list type values only at the end of
            the list of values and not in the middle or at the beginning (this
            restriction will cover the list of valid enum values as well).
        5.  These same restrictions will apply recursively to all property
            schema definitions listed in a dictionary type policy.
    8.  Released dict policies cannot remove or modify any existing keys in
        their schema. They can only add new keys to the schema.
        1.  Dictionary policies can have some of their "required" fields removed
            in order to be less restrictive.

4.  A policy is **partially released**  if both `supported_on` and
    `future_on` list are not empty.

5.  The **partially released** policy is considered as a **released** policy
    and only the `future_on` list can be modified freely. However, any
    platform in the `supported_on` list cannot be moved back to the `future_on`
    list.

6.  Compatibility checks can be bypassed by adding
    `BYPASS_POLICY_COMPATIBILITY_CHECK=<reason>` to the CL description. However,
    this should be used sparingly and only if there is a strong reason to do so.