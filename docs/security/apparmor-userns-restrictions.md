# AppArmor User Namespace Restrictions vs. Chromium Developer Builds

## Short summary

If you want to run developer builds of Chromium/Chrome on Ubuntu 23.10+
(or possibly other Linux distros in the future), you'll need to either globally
or selectively disable an Ubuntu security feature.

## How can I run developer builds or chromium builds I downloaded from the internet?

### Option 1, the easiest way

The easiest way is to disable Ubuntu's security feature globally by running
these commands in a terminal:
```
echo 0 | sudo tee /proc/sys/kernel/apparmor_restrict_unprivileged_userns
```

but note that this disables a useful Ubuntu security feature.

To make this setting persist across reboots, create a new file in
`/etc/sysctl.d`, for example:

```
echo kernel.apparmor_restrict_unprivileged_userns=0 | sudo tee /etc/sysctl.d/60-apparmor-namespace.conf
```

### Option 2, a safer way

A slightly safer way is to write an AppArmor profile that allows running any
binary named "chrome" under your chromium build directory:
```
export CHROMIUM_BUILD_PATH=/@{HOME}/chromium/src/out/**/chrome
cat | sudo tee /etc/apparmor.d/chrome-dev-builds <<EOF
abi <abi/4.0>,
include <tunables/global>

profile chrome $CHROMIUM_BUILD_PATH flags=(unconfined) {
  userns,

  # Site-specific additions and overrides. See local/README for details.
  include if exists <local/chrome>
}
EOF
sudo service apparmor reload  # reload AppArmor profiles to include the new one

```

Note that an attacker with the ability to create an executable called `chrome`
anywhere in the above directory will be able to bypass Ubuntu's security
mechanism.

You can change `CHROMIUM_BUILD_PATH` to anything you like. ** matches any part
of a path, * matches one component of a path. Other options of described in
`man apparmor.d` under the GLOBBING section.

### Option 3, the safest way

If you have installed Google Chrome, the setuid sandbox helper (the old version
of the sandbox) is available at `/opt/google/chrome/chrome-sandbox`. You can
tell developer builds to use it by putting the following in your `~/.bashrc`:
```
export CHROME_DEVEL_SANDBOX=/opt/google/chrome/chrome-sandbox
```

Ubuntu's packaged version of chromium will not install the setuid sandbox
helper (it's a snap package that disables the ubuntu security feature at
runtime for its installed version of chromium).

If you have not installed Google Chrome, but you do have a chromium source
checkout, you can build the SUID sandbox helper yourself and install it. This
is the old version of the sandbox, but should work without disabling any Ubuntu
security features. See [Linux SUID Sandbox Development]
(https://chromium.googlesource.com/chromium/src/+/main/docs/linux/suid_sandbox_development.md)
for instructions. This should work permanently.

The older version of the sandbox may be slightly weaker, and involves installing
a setuid binary.

## Why does this only affect developer builds?

Ubuntu ships with an AppArmor profile that applies to Chrome stable binaries
installed at `/opt/google/chrome/chrome` (the default installation path). This
policy is stored at `/etc/apparmor.d/chrome`.

## What if I don't have root access to the machine and can't install anything?

You will need to run developer builds with the `--no-sandbox` command line flag,
but be aware that this disables critical security features of Chromium and
should never be used when browsing the open web.

## Technical details

Our primary sandbox no longer works on developer builds on some Linux
distributions, namely Ubuntu, due to a security feature that restricts access
to a powerful kernel feature, user namespaces. User namespaces are used by
Chromium (and many containerization applications) to restrict access to the
filesystem without requiring root privileges or a setuid binary. For a while,
user namespaces have been available to unprivileged (e.g. non-root) users on
most Linux distros, but they exposed a lot of extra kernel attack sruface. For
more details, see Ubuntu's announcement at
https://ubuntu.com/blog/ubuntu-23-10-restricted-unprivileged-user-namespaces.

Individual binaries can be allowlisted by filepath using root-owned AppArmor
profiles stored in `/etc/apparmor.d/`. Ubuntu ships with an AppArmor profile
that applies to Chrome stable binaries installed at
`/opt/google/chrome/chrome` (the default installation path). Ubuntu's packaged
version of Chromium is a snap package, and snap generates an AppArmor profile
at runtime that allows usage of user namespaces.
