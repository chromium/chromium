# Policy Settings in Chrome

## Summary

Chrome exposes a different set of configurations to administrators. These
configurations are called policy and they give administrators more advanced
controls than the normal users. With different device management tools,
an administrator can deliver these policies to many users. Here is the
[help center article](https://support.google.com/chrome/a/answer/9037717?hl=en)
that talks about Chrome policy and its deployment.

## Do I need a policy

Usually you need a policy when

-   Launching a new feature. Create a policy so that the admin can disable or
    enable the feature for all users.

-   Deprecating an old feature. Create a policy to give enterprise users more
    time to migrate away from the feature.

**To read more about best practices for shipping enterprise friendly features,
please read [this article](https://www.chromium.org/developers/enterprise-changes/).**

## Adding a new policy

1.  Design the policy, decide policy name, type, function, etc.
    - Please read [policy_design.md](./policy_design.md) for more information.
    - If you are adding support for a GenAI policy, please also read the
      internal [instructions](http://go/genai-chrome-policy-instructions) for
      new GenAI policies specifically.
2.  Declare the policy in the [policies.yaml](https://cs.chromium.org/chromium/src/components/policy/resources/templates/policies.yaml) file.
    -   This file contains the policy names and their ids.
3.  If you need to add a new policy group, create a directory with the name of
    that group under [policy group](https://cs.chromium.org/chromium/src/components/policy/resources/templates/policy_definitions/).
    -  Inside the newly created directory, create a `.group.details.yaml` file
       with the caption and description of the group. This group is used for
       documentation and policy template generation, so it is recommended to
       group policies in meaningful groups.
    -  Use [.group.details.yaml](https://cs.chromium.org/chromium/src/components/policy/resources/new_policy_templates/.group.details.yaml)
       as a templates for group definition.
4.  Create a file named `PolicyName.yaml` under the appropriate [policy group](https://cs.chromium.org/chromium/src/components/policy/resources/templates/policy_definitions/).
    Please use [policy.yaml](https://cs.chromium.org/chromium/src/components/policy/resources/new_policy_templates/policy.yaml) to start off your policy.
    -   This file contains meta-level descriptions of all policies and is used
        to generate code, policy templates as well as documentation. Please make
        sure you get the version and feature flags (such as `dynamic_refresh`
        and `supported_on`) right. More details on the fields can be found in
        [policy.yaml](https://cs.chromium.org/chromium/src/components/policy/resources/new_policy_templates/policy.yaml).
    -   See [description_guidelines.md](description_guidelines.md)
        for additional guidelines when creating a description, including how
        various products should be referenced.
    -   Optional fields can be skipped when they are set to the default value.
5.  Create a policy atomic group.
    -  If you are adding multiple policies that are closely related and interact
      with each other, you should put them in policy atomic group. An atomic
      policy group is used in the Chromium code and affects how policies are
      applied. When enabled by the admin, this ensures that policies from an
      atomic group get their values from the same source and are not a mix of
      policies from multiple sources. This [feature](https://chromeenterprise.google/intl/en_ca/policies/atomic-groups/)
      is controlled by the policy [PolicyAtomicGroupsEnabled](https://chromeenterprise.google/intl/en_ca/policies/#PolicyAtomicGroupsEnabled).

        -  Declare the atomic group in the [policies.yaml](https://cs.chromium.org/chromium/src/components/policy/resources/templates/policies.yaml) file.
    -  Create a `policy_atomic_groups.yaml` file in the group where you added the
       policies if it does not already exist.
       You may use [policy_atomic_groups.yaml](https://cs.chromium.org/chromium/src/components/policy/resources/new_policy_templates/policy_atomic_groups.yaml) as reference.
6.  Create a preference and map the policy value to it.
    -   All policy values need to be mapped into a prefs value before being used
        unless the policy is needed before PrefService initialization.
    -   To map the policy:
        1.  [Create a prefs and register the prefs in **Local State** or
            **Profile Prefs**.](../../chrome/browser/prefs/README.md)
            Please note that, this must match the `per_profile` attribute in the
            `YourPolicyName.yaml`. We also strongly encourage developers to
            register the prefs with **Profile Prefs** if possible, because
            this gives admin more flexibility of policy setup.
        2.  Most policies can be mapped to prefs with `kSimplePolicyMap` in
            [configuration_policy_handler_list_factory.cc](https://cs.chromium.org/chromium/src/chrome/browser/policy/configuration_policy_handler_list_factory.cc?type=cs&q=kSimplePolicyMap&g=0&l=150). If the policy needs additional verification or processing, please
            implement a `ConfigurationPolicyHandler` to do so.
        3.  Test the mapping by adding PolicyName.json  under
            [policy/test/data/pref_mapping](https://cs.chromium.org/chromium/src/components/policy/test/data/pref_mapping) (see [instructions](https://cs.chromium.org/chromium/src/docs/enterprise/policy_pref_mapping_test.md)).
        4.  iOS platform has its own
            [configuration_policy_handler_list_factory.mm](https://source.chromium.org/chromium/chromium/src/+/main:ios/chrome/browser/policy/model/configuration_policy_handler_list_factory.mm)
            and
            [policy/pref_mapping](https://source.chromium.org/chromium/chromium/src/+/main:ios/chrome/test/data/policy/pref_mapping)
            file.
7.  Disable the user setting UI when the policy is applied.
    -   If your feature can be controlled by GUI in `chrome://settings`, the
        associated option should be disabled when the policy controlling it is
        managed.
        -   `PrefService:Preference::IsManaged` reveals whether a prefs value
            comes from policy or not.
        -   The setting needs an
            [indicator](https://cs.chromium.org/chromium/src/ui/webui/resources/images/business.svg)
            to tell users that the setting is enforced by the administrator.
        -   There are more information and util functions can be found [here](https://source.chromium.org/chromium/chromium/src/+/main:ui/webui/resources/cr_elements/policy/).
8.  Support `dynamic_refresh` if possible.
    -   We strongly encourage developers to make their policies support this
        attribute. It means the admin can change the policy value and Chrome
        will honor the change at run-time without requiring a restart of the
        browser. ChromeOS does not always support non-dynamic profile policies.
        Please verify with a ChromeOS policy owner if your profile policy does
        not support dynamic refresh on ChromeOS.
    -   Most of the time, this requires a
        [PrefChangeRegistrar](https://cs.chromium.org/chromium/src/components/prefs/pref_change_registrar.h)
        to listen to the preference change notification and update UI or
        browser behavior right away.
9.  Adding a device policy for ChromeOS.
    -   Most policies that are used by the browser can be shared between desktop
        and ChromeOS. However, you need a few additional steps for a device
        policy on ChromeOS.
        -   Add a field for your policy in
            `components/policy/proto/chrome_device_policy.proto`. Please note
            that all proto fields are optional.
        -   Update
            `chrome/browser/ash/policy/core/device_policy_decoder.{h,cc}`
            for the new policy.
10. Test the policy.
    -   Add a test to verify the policy. You can add a test in
        `chrome/browser/policy/<area>_policy_browsertest.cc` or with the policy
        implementation. For example, a network policy test can be put into
        `chrome/browser/net`. Ideally, your test would set the policy, fire up
        the browser, and interact with the browser just as a user would do to
        check whether the policy takes effect.
11. Manually testing your policy.
    -   This [Internal Doc](go/chrome-with-policies) contains the most up to date
        instructions for testing policies. However, the following docs are
        still useful as public references.
    -   Windows: The simplest way to test is to write the registry keys manually
        to `Software\Policies\Chromium` (for Chromium builds) or
        `Software\Policies\Google\Chrome` (for Google Chrome branded builds). If
        you want to test policy refresh, you need to use group policy tools and
        gpupdate; see
        [Windows Quick Start](https://www.chromium.org/administrators/windows-quick-start).
    -   Mac: See
        [Mac Quick Start](https://www.chromium.org/administrators/mac-quick-start)
        (section "Debugging")
    -   Linux: See
        [Linux Quick Start](https://www.chromium.org/administrators/linux-quick-start)
        (section "Set Up Policies")
    -   ChromeOS and Android are more complex to test, as a full end-to-end
        test requires network transactions to the policy test server.
        Instructions on how to set up the policy test server and have the
        browser talk to it are here:
        [Running the cloud policy test server](https://www.chromium.org/developers/how-tos/enterprise/running-the-cloud-policy-test-server).
        If you'd just like to do a quick test for ChromeOS, the Linux code is
        also functional on CrOS, see
        [Linux Quick Start](https://www.chromium.org/administrators/linux-quick-start).
12. If you are adding a new policy that supersedes an older one, verify that the
    new policy works as expected even if the old policy is set. And if the new
    policy has any interactions with the other policies, make sure to document
    and test all the possible combinations.

## Policy Launch, modification and deprecation

Please read [life of a policy](./life_of_a_policy.md) for more information.

## Examples

- Here is an example for adding a new policy. It's a good, simple place to
  get started:
  [https://chromium-review.googlesource.com/c/chromium/src/+/4004453](https://chromium-review.googlesource.com/c/chromium/src/+/4004453)
- This is an example for the clean-up CL needed to remove a deprecated, escape
  hatch policy.
  [https://chromium-review.googlesource.com/c/chromium/src/+/5483886](https://chromium-review.googlesource.com/c/chromium/src/+/5483886)
- This is an example of a CL that sets the expiration date of a policy in a
  future milestone:
  [https://chromium-review.googlesource.com/c/chromium/src/+/4022055](https://chromium-review.googlesource.com/c/chromium/src/+/4022055)

## Cloud Policy

**For Googlers only**: The Cloud Policy will be maintained by the Admin console
team.
[See instructions here](https://docs.google.com/document/d/1QgDTWISgOE8DVwQSSz8x5oKrI3O_qAvOmPryE5DQPcw/edit?usp=sharing)
on how to update the Cloud Policy.

## Post policy update

Once the policy is added or modified, nothing else needs to be taken
care of by the Chromium developers. However, there are a few things that will be
updated based on the yaml file. Please note that there is no ETA for
everything listed below.

* [Policy templates](https://dl.google.com/dl/edgedl/chrome/policy/policy_templates.zip)
  will be updated automatically.
* [Policy documentation](https://cloud.google.com/docs/chrome-enterprise/policies/)
  will be updated automatically.

------

### Additional Notes

1. The `future_on` flag can disable policy on Beta of Stable channel only if the
   policy value is copied to `PrefService` in Step 3 of **Adding a new policy**.
