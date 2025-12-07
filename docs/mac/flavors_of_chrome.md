# Flavors of Google Chrome for the Mac

Google Chrome for the Mac comes in a wide variety of flavors that differ widely;
this document aims to document those differences.

All references to Google Chrome in this document are specifically to the version
for the Mac. Other platforms may have different flavors available.

## Fundamentals

### Channels vs flavors

A channel of Chrome is a role seen by the user: Extended Stable vs Stable vs
Beta vs Dev vs Canary. These channels exist on a continuum from “what is
expected and reliable” to “what is new and excited but less thoroughly tested.”

A flavor of Chrome is a specific combination of a build configuration and a
repository branch.

Channels and flavors are different. It is currently the case that one channel
comprises two different flavors, that one flavor is deployed for use in two
different channels, and that there is a flavor that isn’t used for a channel at
all.

### Non–side-by-side vs side-by-side flavors

There are two distinct flavors of Chrome that are both called “Beta,” and two
distinct flavors of Chrome that are both called “Dev.” Both Beta and Dev
channels of Chrome have a “non–side-by-side” (“non-SxS”) flavor, and a
“side-by-side” (“SxS”) flavor. The difference between those two is that a
side-by-side flavor of Chrome has a distinct bundle ID and a distinct user data
directory location. Because it has those two properties, it is capable of being
run at the same time as a Stable channel instance. In contrast, a
non–side-by-side flavor of Chrome shares the same bundle ID and user data
directory location with the Stable channel, and therefore cannot be run at the
same time as a Stable channel instance.

The easiest way to visually distinguish a side-by-side Beta or Dev Chrome from a
non–side-by-side one is that the side-by-side flavor has an icon that specifies
“Beta” or “Dev”.

Because the side-by-side aspect is defined by whether a flavor shares or doesn’t
share a bundle ID and user data directory location with the Stable flavor, the
Stable flavor is considered to neither be a side-by-side flavor nor a
non–side-by-side flavor.

The Canary channel only exists in a side-by-side flavor. There is no
non–side-by-side flavor of Canary.

## From different perspectives

### Normal end-users

A normal end-user can choose to download and install any or all of four channels
of Chrome: The [Stable channel](https://www.google.com/chrome/), the [Beta
channel](https://www.google.com/chrome/beta/), the [Dev
channel](https://www.google.com/chrome/dev/), and the [Canary
channel](https://www.google.com/chrome/canary/).

As of [late 2020](https://crbug.com/40122449), the Beta channel and Dev channel
downloads offered to normal end-users were changed to be the side-by-side
flavors. Beta channel or Dev channel installations before that time were the
non–side-by-side flavors, and all such installations continue to update as those
non–side-by-side flavors.

It is possible to easily [migrate from Mac to
Mac](https://support.apple.com/en-us/102613), and therefore the expectation is
that there will continue to exist normal end-users with historical
non–side-by-side installations of Chrome essentially forever.

### Enterprise admins

Enterprise admins have the ability to deploy specific release channels to their
users, and switch their users between channels while maintaining their data.
They have [four channel
options](https://support.google.com/chrome/a?p=chrome_browser_release_channels)
for doing so: Extended Stable, Stable, Beta, and Dev.

If an admin deploys a Beta or Dev channel Chrome to the user, a non–side-by-side
flavor is used. The fact that it shares the bundle ID and user data directory
location with the Stable channel is key to making this work, as sharing those
properties means that it can be used as a drop-in replacement. This also
explains why the Canary channel is not currently available as an option for a
selectable channel, as it is not available in a non–side-by-side flavor.

While an admin can deploy an Extended Stable channel Chrome to their users,
Extended Stable is not actually a separate flavor of Chrome. An Extended Stable
channel Chrome is the same flavor used for the normal Stable channel of Chrome,
and what makes it different is the updater configuration.

### Web developers

Web developers need a flavor of Chrome for testing purposes, and [Chrome for
Testing](https://developer.chrome.com/blog/chrome-for-testing) is that flavor.
What makes Chrome for Testing unique is that it is effectively a sibling product
to Chrome; it is not a channel of Chrome. It is not considered to be a
side-by-side flavor, as that property is only meaningful for channels of Chrome,
but Chrome for Testing is not a Chrome channel.

## Summary chart

| Flavor                | Bundle ID                       | Creator Code | User Data Directory location                          | Channel(s)              |
|-----------------------|---------------------------------|--------------|-------------------------------------------------------|-------------------------|
| Chrome Stable         | `com.google.Chrome`             | `'rimZ'`     | `~/Library/Application Support/Google/Chrome/`        | Stable, Extended Stable |
| Chrome Beta (non-SxS) | `com.google.Chrome`             | `'rimZ'`     | `~/Library/Application Support/Google/Chrome/`        | Beta                    |
| Chrome Dev (non-SxS)  | `com.google.Chrome`             | `'rimZ'`     | `~/Library/Application Support/Google/Chrome/`        | Dev                     |
| Chrome Beta (SxS)     | `com.google.Chrome.beta`        | `'Late'`     | `~/Library/Application Support/Google/Chrome Beta/`   | Beta                    |
| Chrome Dev (SxS)      | `com.google.Chrome.dev`         | `'Prod'`     | `~/Library/Application Support/Google/Chrome Dev/`    | Dev                     |
| Chrome Canary         | `com.google.Chrome.canary`      | `'Pipi'`     | `~/Library/Application Support/Google/Chrome Canary/` | Canary                  |
| Chrome for Testing    | `com.google.chrome.for.testing` | `'rimZ'`     | `~/Library/Application Support/Google/Chrome/`        | _none_                  |
