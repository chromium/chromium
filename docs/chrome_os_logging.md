# Chrome Logging on Chrome OS

## Locations

Messages written via the logging macros in [base/logging.h] end up in different
locations depending on Chrome's state:

* `/var/log/ui/ui.LATEST` contains data written to stdout and stderr by Chrome
  (and technically also [session_manager]). This generally comprises messages
  that are written very early in Chrome's startup process, before logging has
  been initialized.
* `/var/log/chrome/chrome` contains messages that are written before a user has
  logged in. It also contains messages written after login on test images, where
  chrome runs with `--disable-logging-redirect`.
* `/home/chronos/user/log/chrome` contains messages that are written while a
  user is logged in on non-test images. Note that this path is within the user's
  encrypted home directory and is only accessible while the user is logged in.

All of the above files are actually symlinks. Older log files can be found
alongside them in the same directories.

Also notable is `/var/log/messages`. This file contains general syslog messages,
but it also includes messages written by `session_manager` that may be useful in
determining when or why Chrome started or stopped.

## Severity

By default, only messages logged at severity `WARNING` or higher are written to
disk. More concretely, `LOG(INFO)` messages are discarded.

To enable `LOG(INFO)` message, pass `--log-level=0` to Chrome. See the
[Passing Chrome flags from session_manager] document for more details, and
specifically the `/etc/chrome_dev.conf` mechanism that can be used to
change flags on development devices.

## Verbose Logging

When actively debugging issues, Chrome's `--vmodule` flag is sometimes used to
temporarily log messages at lower severities for particular modules.

[base/logging.h]: ../base/logging.h
[session_manager]: https://chromium.googlesource.com/chromiumos/platform2/+/main/login_manager/
[Passing Chrome flags from session_manager]: https://chromium.googlesource.com/chromiumos/platform2/+/main/login_manager/docs/flags.md
