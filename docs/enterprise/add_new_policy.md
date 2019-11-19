# Policy Settings in Chrome

## Terms

-   User Policy: The most common kind. Associated with a user login.
-   Device Policy: (a.k.a. cloud policy) ChromeOS only. Configures device-wide
    settings and affect unmanaged (i.e. some random gmail) users. Short list
    compared to user policy. The most important device policy controls which
    users can log into the device.

## Adding new policy settings

This section describes the steps to add a new policy setting to Chromium, which
administrators can then configure via Windows Group Policy, the G Suite Admin
Console, etc. Administrator documentation about setting up Chrome management is
[here](https://www.chromium.org/administrators) if you're looking for
information on how to deploy policy settings to Chrome.

1.  Think carefully about the name and the desired semantics of the new policy:
    -   Chose a name that is consistent with the existing naming scheme. Prefer
        "XXXEnabled" over "EnableXXX" because the former is more glanceable and
        sorts better.
    -   Consider the foreseeable future and try to avoid conflicts with possible
        future extensions or use cases.
    -   Negative policies (*Disable*, *Disallow*) are verboten because setting
        something to "true" to disable it confuses people.
2.  Wire the feature you want to be controlled by policy to PrefService, so a
    pref can be used to control your feature's behavior in the desired way.
    -   For existing command line switches that are being turned into policy,
        you will want to modify the `ChromeCommandLinePrefStore` in
        [chrome/browser/prefs/chrome_command_line_pref_store.cc](https://cs.chromium.org/chromium/src/chrome/browser/prefs/chrome_command_line_pref_store.cc?sq=package:chromium&dr=CSs&g=0)
        to set the property appropriately from the command line switch (the
        managed policy will override this value from the command line
        automagically when policy is set if you do it this way).
3.  Add a policy to control the pref:
    -   [components/policy/resources/policy_templates.json](https://cs.chromium.org/chromium/src/components/policy/resources/policy_templates.json) -
        This file contains meta-level descriptions of all policies and is used
        to generated code, policy templates (ADM/ADMX for windows and the
        application manifest for Mac), as well as documentation. When adding
        your policy, please make sure you get the version and features flags
        (such as dynamic_refresh and supported_on) right, since this is what
        will later appear on
        [http://dev.chromium.org/administrators/policy-list-3](http://dev.chromium.org/administrators/policy-list-3).
        The textual policy description should include the following:
        -   What features of Chrome are affected.
        -   Which behavior and/or UI/UX changes the policy triggers.
        -   How the policy behaves if it's left unset or set to invalid/default
            values. This may seem obvious to you, and it probably is. However,
            this information seems to be provided for Windows Group Policy
            traditionally, and we've seen requests from organizations to
            explicitly spell out the behavior for all possible values and for
            when the policy is unset.
    -   [chrome/browser/policy/configuration_policy_handler_list_factory.cc](https://cs.chromium.org/chromium/src/chrome/browser/policy/configuration_policy_handler_list_factory.cc) -
        for mapping the policy to the right pref.
4.  If your feature can be controlled by GUI in `chrome://settings`, then you
    will want `chrome://settings` to disable the GUI for the feature when the
    policy controlling it is managed.
    -   There is a method on PrefService::Preference to ask if it's managed.
    -   You will also want `chrome://settings` to display the "some settings on
        this page have been overridden by an administrator" banner. If you use
        the pref attribute to connect your pref to the UI, this should happen
        automagically. NB: There is work underway to replace the banner with
        setting-level indicators. Once that's done, we'll update instructions
        here.
5.  Wherever possible, we would like to support dynamic policy refresh, that is,
    the ability for an admin to change policy and Chrome to honor the change at
    run-time without requiring a restart of the process.
    -   This means that you should listen for preference change notifications
        for your preference.
    -   Don't forget to update `chrome://settings` when the preference changes.
        Note that for standard elements like checkboxes, this works out of the
        box when you use the `pref` attribute.
6.  If you’re adding a device policy for Chrome OS:
    -   Add a message for your policy in
        components/policy/proto/chrome_device_policy.proto.
    -   Add the end of the file, add an optional field to the message
        ChromeDeviceSettingsProto.
    -   Make sure you’ve updated
        chrome/browser/chromeos/policy/device_policy_decoder_chromeos.{h,cc} so
        the policy shows up on the chrome://policy page.
7.  Build the `policy_templates` target to check that the ADM/ADMX, Mac app
    manifests, and documentation are generated correctly.
    -   The generated files are placed in `out/Debug/gen/chrome/app/policy/` (on
        Linux, adjust for other build types/platforms).
8.  Add an entry for the new policy in
    `chrome/test/data/policy/policy_test_cases.json`.
9.  By running `python tools/metrics/histograms/update_policies.py`, add an
    entry for the new policy in `tools/metrics/histograms/enums.xml` in the
    EnterprisePolicies enum. You need to check the result manually.
10. Add a test that verifies that the policy is being enforced in
    `chrome/browser/policy/policy_browsertest.cc`. Ideally, your test would set
    the policy, fire up the browser, and interact with the browser just as a
    user would do to check whether the policy takes effect. This significantly
    helps Chrome QA which otherwise has to test your new policy for each Chrome
    release.
11. Manually testing your policy.
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
    -   Chrome OS and Android are more complex to test, as a full end-to-end
        test requires network transactions to the policy test server.
        Instructions for how to set up the policy test server and have the
        browser talk to it are here:
        [Running the cloud policy test server](https://www.chromium.org/developers/how-tos/enterprise/running-the-cloud-policy-test-server).
        If you'd just like to do a quick test for Chrome OS, the Linux code is
        also functional on CrOS, see
        [Linux Quick Start](https://www.chromium.org/administrators/linux-quick-start).
12. If you are adding a new policy that supersedes an older one, verify that the
    new policy works as expected even if the old policy is set (allowing us to
    set both during the transition time when Chrome versions honoring the old
    and the new policies coexist).
13. If your policy has interactions with other policies, make sure to document,
    test and cover these by automated tests.

## Examples

Here's a CL that has the basic infrastructure work required to add a policy for
an already existing preference. It's a good, simple place to get started:
[http://codereview.chromium.org/8395007](http://codereview.chromium.org/8395007).

## Modifying existing policies

If you are planning to modify an existing policy, please send out a one-pager to
client- and server-side stakeholders explaining the planned change.

There are a few noteworthy pitfalls that you should be aware of when updating
code that handles existing policy settings, in particular:

- Make sure the policy meta data is up-to-date, in particular supported_on, and
the feature flags.
- In general, don’t change policy semantics in a way that is incompatible
(as determined by user/admin-visible behavior) with previous semantics. **In
particular, consider that existing policy deployments may affect both old and
new browser versions, and both should behave according to the admin's
intentions**.
- **An important pitfall is that adding an additional allowed
value to an enum policy may cause compatibility issues.** Specifically, an
administrator may use the new policy value, which makes older Chrome versions
that may still be deployed (which don't understand the new value) fall back to
the default behavior. Carefully consider if this is OK in your case. Usually,
it is preferred to create a new policy with the additional value and deprecate
the old one.
- Don't rely on the cloud policy server for policy migrations because
this has been proven to be error prone. To the extent possible, all
compatibility and migration code should be contained in the client.
- It is OK to expand semantics of policy values as long as the previous policy
description is compatible with the new behavior (see the "extending enum"
pitfall above however).
- It is OK to update feature implementations and the policy
description when Chrome changes as long as the intended effect of the policy
remains intact.
- The process for removing policies is to deprecate them first,
wait a few releases (if possible) and then drop support for them. Make sure you
put the deprecated flag if you deprecate a policy.

## Updating Policy List in this Wiki

Steps for updating the policy list on
[http://dev.chromium.org/administrators/policy-list-3](http://dev.chromium.org/administrators/policy-list-3):

1.  Use a recent checkout to build the GN target `policy_templates` with
    `is_official_build=true` and `is_chrome_branded=true`.
2.  Edit page
    [http://dev.chromium.org/administrators/policy-list-3](http://dev.chromium.org/administrators/policy-list-3)
    and select "Edit HTML", therein delete everything except "Last updated for
    Chrome XX." and set XX to the latest version that has been officially
    released.
3.  Open
    `<outdir>/gen/chrome/app/policy/common/html/en-US/chrome_policy_list.html`
    in a text editor.
4.  Cut&paste everything from the text editor into the wiki.
5.  Add some <p>...</p> to format the paragraphs at the head of the page.

## Updating ADM/ADMX/JSON templates

The
[ZIP file of ADM/ADMX/JSON templates and documentation](https://dl.google.com/dl/edgedl/chrome/policy/policy_templates.zip)
is updated upon every push of a new Chrome stable version as part of the release
process.

## Updating YAPS

Once your CL with your new policy lands, the next proto sync (currently done
every Tuesday by hendrich@) will pick up the new policy and add it to YAPS. If
you want to use your unpublished policies with YAPS during development, please
refer to the "Custom update to the Policy Definitions" in
(https://sites.google.com/a/google.com/chrome-enterprise-new/faq/using-yaps).

## Updating Admin Console

[See here for instructions](https://docs.google.com/document/d/1QgDTWISgOE8DVwQSSz8x5oKrI3O_qAvOmPryE5DQPcw/edit)
on adding the policy to Admin Console (Google internal only).
