# How to design an enterprise policy

## Summary

Chrome exposes a different set of configurations to administrators. These
configurations are called policies and they give administrators more advanced
controls than the standard users. With different device management tools, an
administrator can deliver these policies to many users. Here is the [help center article](https://support.google.com/chrome/a/?p=policy_order)
that talks about Chrome policy and its deployment.


## Do I need a policy

Not every single Chrome update needs to be guarded with a policy. In some cases,
enterprise users can accept new features just like consumer users. However,
consider adding new policy if

* Your feature will break an existing CUJ (Critical User Journey).
* Your feature will introduce changes in some critical areas like security,
  networking.
* Your feature can be configured by end users.
* Your feature has compliance requirements, e.g. upload data to Google owned
  servers.
* You are removing an existing feature from Chrome.

**To read more about best practices for shipping enterprise friendly features
read [this article](https://www.chromium.org/developers/enterprise-changes/).**

**To learn more about policy implementation details, please read [add_new_policy.md](./add_new_policy.md)**.

And if you are not sure, don’t hesitate to contact the [Chrome enterprise team](mailto:chromium-enterprise@chromium.org)).


## Design a new policy

### Before Starting

Similar to the switches on the chrome://settings page, enterprise policies are
designed to provide options to our users to customize Chrome’s behaviors.

In some cases, a simple kill switch is a good starting point. It allows admins
to force a feature they really need or block one that they can’t have due to
privacy or security concerns. However, a boolean won’t always provide enough
flexibility and may turn enterprise users away. In other words, providing
granular control would be a better approach here. It allows enterprise users
access to a feature while administrators can still meet all compatibility
requirements. Sometimes, a policy can even be used to provide an enhanced
version of the feature to enterprise users.

For example, [ExtensionSettings](https://chromeenterprise.google/policies/#ExtensionSettings)
allows admins to block extensions based on their permissions or update URLs
while still giving users the ability to install extensions that they like and
admins are comfortable with.

Another example is [BrowserSignin](https://chromeenterprise.google/policies/#BrowserSignin)
policy which provides an additional force-sign-in feature which is not available
for consumer users.

Think about CUJ(Critical User Journey), collect information to understand how
administrators and enterprise users will use this feature. They can help you to
design a policy.


### Define a policy

Each Chrome policy must be defined in the [Chromium code base](https://cs.chromium.org/chromium/src/components/policy/resources/templates/policy_definitions/)
which contains all metadata of a policy.


#### Name

Policy name is a short phrase that briefly describes what the policy does.


* Do **NOT** use negative words like _Disabled_ or _Disallowed_. It will lead to
  double negatives and confuse people. Instead, prefer a policy name like
  FooEnabled (which can be set to false in cases where a feature should be
  disabled).
* Avoid long policy names. Policy names don't have to cover everything. There is
  a documentation section for all the details.
* Avoid internal codewords. Policy names are public information. Make sure
  keywords can be searched and understood by external people.

Searching for a similar pattern from existing [policies list](https://chromeenterprise.google/policies/)
is always a good strategy to find a good name. However, be careful about some
ancient policies that were added a long time ago. We may already abandon those
naming patterns, but keep the policies only because of backward compatibility.
For example, [SyncDisabled](https://chromeenterprise.google/policies/#SyncDisabled)
was added in Chrome 8, but we keep its name even though _Disabled_ is banned.


#### Type

There are 6 major types of policies, listed below.


##### Boolean

This is the simplest policy type that defines 3 states: enabled, disabled and
not set. Not-set should have the same behavior as consumer users.

Example: [CloudReportingEnabled](https://chromeenterprise.google/policies/#CloudReportingEnabled)


##### Enum

A policy type provides more than 3 states for the admin to choose from.

Example: [BrowserSignin](https://chromeenterprise.google/policies/#BrowserSignin)

If multiple options can be chosen at the same time, use string-enum-list type.

Example: [ExtensionAllowedTypes](https://chromeenterprise.google/policies/#ExtensionAllowedTypes)


##### Integer

A policy that accepts any integer as input. It can be used to define a period of
time or size of a folder. The integer can't be negative and must be less than
2^32.

In most cases, there are two things to consider.

* Choose a proper interval.
* Choose a proper unit.

Unit:

The enterprise team used to use the minimum unit for policy to give enterprise
more flexibility. However, that may require admin to put an unnecessary large
number for certain policies which is error-prone.

Now we ask people to choose more proper units. For example, instead of
milliseconds, hours may be a better choice, if most people won’t care about the
differences between 1 hour and 59 minutes.

Example: [CloudReportingUploadFrequency](https://chromeenterprise.google/policies/#CloudReportingUploadFrequency)
and [DiskCacheSize](https://chromeenterprise.google/policies/#DiskCacheSize)


##### String

A policy that accepts any string as input.

Starting from this type, admin can put anything as policy input and error
handling become more important.

* If a policy input is partially invalid, can we still process the good part?
* Presenting good error messages to help admins fix the issues.

Note that setting an empty string *must* be treated as not setting the policy
due to the limitation of policy delivery mechanisms on some platforms.

Example [HomepageLocation](https://chromeenterprise.google/policies/#HomepageLocation)


##### List

A policy that accepts a list of strings as input.

Similar to string policy

* Empty list *must* be treated as not set.
* User input validation needs to be handled properly.

In addition to that, depending on the policy implementation, setting a
limitation is necessary when having too many list entries could cause
performance issues. As an example, [URLBlocklist](https://chromeenterprise.google/policies/#URLBlocklist)
policy only accepts 1000 URLs because we need to scan the list for every navigation request.

However, sometimes admins may want to set values more than what we can afford.
In those cases, we will need to redesign the input format. For example,
supporting wildcard characters is a common solution.

Example: [URLBlocklist](https://chromeenterprise.google/policies/#URLBlocklist),
[ExtensionInstallAllowlist](https://chromeenterprise.google/policies/#ExtensionInstallAllowlist)


###### URL list

URL list is a common format of list policy. It allows admins to apply policy
only to certain websites or URLs.

Other than normal list policy, a few more considerations:

* In many cases, URL matching is not necessary. Domain or hostname matching may
  be good enough and much easier to maintain.
* Choose URL matching algorithm:
  * Exact match
  * [Content settings](https://chromeenterprise.google/policies/url-patterns/)
    for content settings
  * [URLBlocklist](https://support.google.com/chrome/a?p=url_blocklist_filter_format)
    style with basic wildcard support.
  * Define your own. Avoid this option if possible. If you have to, make sure
    all edge cases are addressed.



###### Allowlist vs Blocklist

Another common format of list policies. It allows the admin to set exceptions
for certain situations. Usually, we can create them as:

* A standalone allowlist/blocklist policy.
* An on/off boolean policy as default option with another allowlist/blocklist
  policy as exception.
* Allowlist and blocklist policies pair.

If more than one policy is defined for the same feature, please find a proper
solution to resolve the conflict cases.


##### Dictionary

A policy that accepts a complex structure as input. Depending on the
platforms, it could be a JSON string or XML files.

This is the most powerful and complicated policy format. A complicated
dictionary policy may be hard for admins to understand and maintain. In many
cases, splitting a dictionary configuration into multiple simple policies is a
better approach.

Same as String and List policy, an empty dictionary *must* be treated as
not setting the policy.

Also, a complicated dictionary policy may also contain conflict configurations.
Make sure those cases are handled properly.

Example: [ExtensionSettings](https://chromeenterprise.google/policies/#ExtensionSettings)


#### Default Value

The default value defines Chrome’s behavior when policy is not set.


#### Supported Platforms

Always support as many platforms as possible. And unless there are good reasons,
please launch a policy on all platforms at the same time.


#### Feature


* **_Dynamic refresh_** - When set to true, modifying the policy does not
  require relaunching the browser. Support this feature whenever possible.
* **_Per profile_** - When set to true, policy value can be applied to only one
  particular profile. Support this feature whenever possible.
* **_Hidden policies_** - In some cases, you would like to hide the policies
  from most admins. There is no perfect solution as admins can always check the
  source code of Chromium. But we still can hide a policy as much as we can.
  Talk with the Enterprise team for more details.
* **_Recommended vs mandatory_** - Recommended policies mean the admin only sets
  the initial value of the policy, but allows users to modify them later. Most
  policies are mandatory only, but having a recommended support is encouraged.
  However, make sure the user can actually modify that settings. (e.g. Having a
  UI or an extension API)
* **_Device policy_** - A special policy that controls ChromeOS hardware or sign
  in page. If a policy needs to control both browser and ChromeOS device,
  split it into two policies. For example, [MetricsReportingEnabled](https://chromeenterprise.google/policies/#MetricsReportingEnabled)
  vs [DeviceMetricsReportingEnabled](https://chromeenterprise.google/policies/#DeviceMetricsReportingEnabled).
  Device policies’ name always begin with the “Device” prefix.


### More Policy Details

#### Chrome UI

If a setting is controlled by policy and UI at the same time, please make sure
the UI is disabled when the policy is set with a proper message or UI element
which tells end users that the option is controlled by their administrator.

Alert: There might be multiple UI entries, make sure all of them are covered.


#### Admin Console UI

Most of the time, there is no need to worry about the admin console UI. However,
cover this section if your policy is complicated. Talk with
[the server team](https://docs.google.com/document/d/1QgDTWISgOE8DVwQSSz8x5oKrI3O_qAvOmPryE5DQPcw/edit#heading=h.py10m8dh4kvv)
for more details. (Internal link, Googlers only)


#### A group of policies

If multiple policies are added at the same time or there are existing policies
for the same feature, make sure all those policies won’t break each other.

* One policy may provide conflicting settings with another one.
* One policy may have different behavior depending on another one.

Also, if multiple policies are created for the same topic, creating a new
[policy group](https://source.chromium.org/chromium/chromium/src/+/main:components/policy/resources/new_policy_templates/.group.details.yaml)
can help admins find all related policies.


#### Migrating existing policies

In some cases, we would like to replace one policy with a more powerful one. The
old policy will be deprecated in this case. However, for the sake of backward
compatibility, we still need to support the old one.

On the other hand, it’s always nice to think about future expansion when
designing a policy. For example, if there is a chance we want to add more
options to a policy, then enum type will be better than boolean.


#### Privacy & Security

Policy is a very powerful configuration tool that may bring additional security
and privacy concerns. Talk with the Chrome privacy and Chrome security ahead of
time if there is any concern.


##### Security

Will bad actors be interested in your policy? The Enterprise team can provide
additional management checks for the policy by marking it sensitive. But it may
block admins from small companies which didn’t pay for expensive management
systems. In other cases, limiting the ability of policy may also help.


##### Privacy

We would like to protect end users’ privacy as much as we can while fulfilling
admins’ requests. Another area that may need careful balance.


### Launch Policy

One major difference between policy launching and other features is that rollout
control via experimental is not always an option. Majority of admins want their
policies settings to apply to all devices they control.

Usually, policy must be ready and launched before the feature it controls rolls
out.

In other cases, we can use trusted testers to test the policies for a small
number of enterprise customers first. Talk with the Enterprise team for more
details.
