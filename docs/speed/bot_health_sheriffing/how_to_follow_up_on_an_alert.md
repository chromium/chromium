# How to follow up on an alert

**Important**: When making changes to this document, also update duplicate files under the [internal docs](http://goto.google.com/perf-bot-health-sheriffs).

[TOC]

Skim the bug to understand where the last sheriff left things and where you should pick up.

Generally, there are two reasons you might see an alert that a previous sheriff already addressed:

1. The sheriff attempted to fix the alert, but the fix was unsuccessful.

2. The sheriff was unable to fix the alert, but wanted a future sheriff to follow up with it.

Several common scenarios for each, along with suggested responses, are listed below.

## Playbook for common scenarios

### Infra Labs took some action to fix a problem (e.g. restarting the device), but it didn't work.

* Ping the bug and let them know that the device is still having issues. Usually, Infra>Labs knows the obvious next step. For example, if the previous solution was to restart the device, the next step may be to replace the device.

* If neither you nor Infra > Labs knows what to do, escalate the issue [on chat](https://hangouts.google.com/group/gbJlAqcAxqfbHbXH3).

### A previous sheriff identified that a problem was due to an infrastructure outage (e.g. the perf dashboard API is temporarily down) and expected the outage to be over by the time that the snooze expired, but it wasn't.

* Escalate the issue to the team by adding the Speed > Benchmarking component and asking about the issue [on chat]((https://hangouts.google.com/group/gbJlAqcAxqfbHbXH3). Get an estimate of when the issue will be fixed and snooze the alert for that amount of time.

### The bot health sheriff was waiting for a response or action from someone, but it never came.

Escalate by (in increasing order of seriousness):

* Disabling the failing benchmark or story if possible (i.e. it's a non-infra failure)

* Pinging the tracking bug again.

* Pinging the person offline.

* Escalate by asking [on chat](https://hangouts.google.com/group/gbJlAqcAxqfbHbXH3) for help (likely from a Speed Ops manager).

### Other

Escalate by asking [on chat](https://hangouts.google.com/group/gbJlAqcAxqfbHbXH3) for help.
