# Gerrit ReAuth

*** note
**Googlers:**

If you use your @google.com account, or a @chromium.org account linked to your
@google.com account: You already ReAuth during your daily `gcert`, no further
action is required. Feel free to stop reading now.

If you use a @chromium.org account that isn't linked to your google.com account,
with a Google-issued security key, on devices managed by Google (e.g. gLinux),
simply run `git credential-luci reauth`, follow the prompts to complete ReAuth.
You need to ReAuth every 20 hours (just like `gcert`).

If you use a terminal persistence tool, such as screen, tmux, or shpool, refer
to [the internal guide](go/gerrit-reauth#bookmark=id.gohr0ejjvi49) for
additional instructions.

Otherwise, follow this guide to ReAuth locally or remotely.

If you aren't sure if your account is linked, follow
[the steps here](http://go/chromium-account-support#how-can-i-check-if-my-gerrit-accounts-are-linked).

For more information, see this internal doc:
[go/gerrit-reauth](http://go/gerrit-reauth).
***

[TOC]

## Background

To further protect the integrity of Chromium’s codebase and other related
projects, including Git repositories, a significant security enhancement is
being implemented. This enhancement requires all **committers** who write or
review code to utilize a security key for two-factor authentication on their
associated Google account.

This new approach, referred to as ReAuth, mandates a security key tap once every
20 hours to obtain a fresh set of credentials for interactions with Git and
Gerrit. Specifically, actions requiring committer powers, such as reviewing
Change Lists (CLs) for submission and uploading CLs (which counts as the
uploader self-reviewing the CL), will necessitate ReAuth.

The primary goal of this policy is to establish a robust layer of protection
against unauthorized access, significantly diminishing the risk of compromised
accounts, supply chain attacks, and malicious activities stemming from stolen
committer credentials.

## Overview

You are required to ReAuth when using git-cl to upload your change. You
ReAuth to git-cl by running `git credential-luci reauth`.

Gerrit Web UI may show [ReAuth popups](#reauth-in-gerrit-web-ui) when you
perform actions like voting Code-Review or editing change descriptions.
In this case, please follow the popup's instructions.

*** promo
ReAuth is valid for 20 hours, so we recommend ReAuth once when you start your
day with `git credential-luci reauth`.
***

*** note
If you work remotely over SSH or remote desktop, please follow steps in
[ReAuth in git-cl remotely](#ReAuth-in-git_cl-remotely) to setup your
environment.

If you use Linux:

1. You need to install a GUI-based `pinentry` program to enter security key
   PINs. Certain security keys models mandate PIN entry at all times.

1. You might also need to [configure your system](#linux-security-keys-access)
   to make security keys usable.
***

## Prerequisites

### Physical Security Key

You must have a physical
[FIDO security key](https://www.google.com/search?q=FIDO+security+key)
registered with your Google account.

To register a key or check your existing keys, go to
[https://myaccount.google.com/signinoptions/passkeys](https://myaccount.google.com/signinoptions/passkeys)

![Security key registration](./images/gerrit_reauth_key_registration.png)

The line "This key can only be used with a password" indicates a **U2F**
security key. If the line is missing, the key is a **FIDO2** security key.
Please include this info when reporting issues.

*** promo
**Important Note**: Passkeys won't be supported by ReAuth. A physical security
key is required.
***

**If you use Firefox**: You need to **allow** the website to request "extended
information about your security key" when registering your security key (refer
to the screenshot below).
Otherwise the key won't be able to ReAuth (you'll see BAD_REQUEST error in the
log). If you've already registered the key, remove it from the security key
list, then add it again.

![Firefox security key popup](./images/gerrit_reauth_firefox_sk.png)

**If you’re using a Google Workspace account**, make sure
"[2-Step Verification](https://myaccount.google.com/signinoptions/twosv)" is
turned on.

![Two-step verification](./images/gerrit_reauth_2sv.png)

*** note
**Known Issue:** If you sign in to your Google account via an external identity provider
such as **Active Directory, Entra ID, or Okta**, you may see `NO_AVAILABLE_CHALLENGES` error
when you ReAuth immediately after registering your security key.

You may need to **wait for a few hours** before your first ReAuth can proceed. We're still
investigating the cause.
***

### Accurate Timezone / Time

Make sure your device's timezone and time are set correctly.

If you’re behind a corporate network or network proxy, your system’s auto
configured timezone might be incorrect. If this is the case, go to your system’s
settings and set timezone and/or time manually.

### Latest Git

Ensure you have the latest version of Git (or at least later than 2.46.0). Use
the package manager for your system or download from the [Git
website](https://git-scm.com/downloads). (Note: if you are on Ubuntu LTS you may
need to follow the instructions on the Git website to install from PPA)

### Latest depot_tools

Ensure you
[have depot_tools](https://commondatastorage.googleapis.com/chrome-infra-docs/flat/depot_tools/docs/html/depot_tools_tutorial.html#_setting_up)
installed and configured on PATH.

Then run:

```
update_depot_tools
```

### Git config for Gerrit

Make sure your Git is configured for Gerrit. You only need to do this once.

```
git cl creds-check --global
```

Please follow the prompts from the tool and resolve any issues.

### Log into Gerrit

Check if you're already logged in (this is likely if you have already logged
in with depot_tools):

```
git credential-luci info
```

This should print a line containing `email=<your email>`. If not, you'll need to
login first:

```
git credential-luci login
```

### Linux: security keys access

Check depot_tools can access your security keys by running:

```
luci-auth-fido2-plugin --list-devices
```

If the above command lists your security keys, you’re good to go.

If not, you need to configure your Linux system to grant access to security
keys.

The configuration steps vary by Linux distributions. We recommend following
[Yubico’s guide](https://support.yubico.com/hc/en-us/articles/360013708900-Troubleshooting-using-your-YubiKey-with-Linux)
, which we confirmed to be working on Ubuntu 24.04 LTS Desktop.

### Linux: security key PIN entry program

ReAuth doesn't require security key PINs. But PINs entry might be enforced by
the security key manufacturer, or if you have configured your key to do so.

On Linux, you need the `pinentry` program to input PINs. If you don't have this
program, your security key will refuse to complete the ReAuth challenge. You
typically see `BAD_REQUEST` or `PinRequiredError` in the logs depending on the
security key.

For the best experience, we recommend using a **GUI based pinentry** program.

Terminal based pinentry only works with local ReAuth. If you don't need to
ReAuth over SSH, feel free to use one.

To install a GUI-based pinentry program:

* Ubuntu, Debian: `sudo apt install pinentry-gnome3`
* Fedora: `sudo dnf install pinentry-qt`

After installing the package, your system should default to the newly installed
GUI-based pinentry program.

You can check the current pinentry program by running:

```
readlink -f $( which pinentry )
```

The output path's suffix should be a GUI based name, such as "-gnome" or "-qt".

If the above path ends with terminal based name, such as "tty" or "curses", set
`LUCI_AUTH_PINENTRY=pinentry-gnome3` environment variable to override.

## ReAuth in Gerrit Web UI

When performing actions such as voting Code-Review or editing commit
descriptions on Gerrit Web UI, you may see popups like:

![Gerrit UI prompt](./images/gerrit_reauth_ui_prompt.png)

Click "Continue". You'll be asked to touch your security key to perform ReAuth,
after which everything will proceed as normal.

## ReAuth in git-cl locally

This is for performing ReAuth locally, on a machine with your security key
inserted.

First, make sure you have the [latest depot_tools](#latest-depot_tools) and
have [set up Git to access Gerrit](#git-config-for-gerrit), and is
[logged into Gerrit](#log-into-gerrit). If you're using Linux, make sure
[depot_tools can access your security keys](#linux_security-keys-access).

To perform ReAuth, run the following command inside your terminal:

```
git credential-luci reauth
```

You will be prompted to touch your security key. If you see “ReAuth succeed.”,
then it works\!

If it doesn't work, please refer to [Troubleshooting](#troubleshooting) to turn
on debug logs, then retry the command.

## ReAuth in git-cl remotely

This is for completing ReAuth when:

- You plug-in a security key to a local client machine machine
- You SSH or remote desktop into a remote development machine (where the
  chromium/src checkout lives)

First, make sure you have the [latest depot_tools](#latest-depot_tools)
installed on **both local and remote** machines.

If you're using a Linux local machine (i.e. the machine you inserts security
keys into), make sure
[depot_tools can access your security keys](#linux_security-keys-access).

Then, on the remote machine, make sure you have
[set up Git to access](#git-config-for-gerrit) and have
[logged into Gerrit](#log-into-gerrit).

Then, refer to sections below for your SSH or remote desktop workflow.

### I’m using a Linux / Mac client, I want to SSH into Linux

If you’re using a Linux client, please check and make sure
[depot_tools can access your security keys](#linux_security-keys-access).

Then, use `luci-auth-ssh-helper` to SSH into the remote machine. You can
specify SSH options (such as port forwarding) after a double dash.

```
luci-auth-ssh-helper [-- ssh_options...] [user@]host
```

In this SSH session, run the following command to ReAuth:

```
git credential-luci reauth
```

You should be prompted to touch your security key. If you see "ReAuth succeed",
then it works\!

For the first security key touch, there might be a delay before your security
key starts blinking. This is caused by `luci-auth-fido2-plugin` bootstrapping.

### I’m using a Linux / Mac client, I want to remote desktop into Windows

If you’re using a Linux client, ensure you’ve completed
["Linux Client Prerequisites"](#linux-client-prerequisites) and made your
security keys available to applications.

You need a remote desktop client that supports WebAuthn forwarding.

For example,
[Thincast Remote Desktop Client](https://thincast.com/en/products/client)
(available free of charge for non-commercial use):

- On Linux, install the **flatpak version**
  ([instructions](https://thincast.com/en/documentation/tcc/latest/index#install-linux)).
  Snapcraft version doesn’t work with security keys (as of 2025 August)
- On MacOS, download and install the universal dmg package
  ([instructions](https://thincast.com/en/documentation/tcc/latest/index#install-linux))

Then, launch the Thincast remote desktop client, enable the "WebAuthn" option in
"Local Resource \> Local devices and resource \> More…" (refer to screenshots
below).

Click "OK" to save your settings, then go back to the "General" tab, input the
remote desktop server with your development machine’s hostname (or IP address)
and user name, then click "Connect".

![](./images/gerrit_reauth_thincast1.png)

![](./images/gerrit_reauth_thincast2.png)

In the remote desktop session, open a command prompt (CMD), then run the
following command to ReAuth:

```
git credential-luci reauth
```

Wait for your security key to blink, then touch it to complete ReAuth. You
should see "ReAuth succeed" in the command prompt.

For the first security key touch, there might be a delay before your security
key starts blinking. This is caused by `luci-auth-fido2-plugin` bootstrapping.

#### I’m using a Windows client, I want to SSH into Linux

First, start `luci-auth-ssh-helper` in daemon mode on a TCP port (we use 10899
in the example). The helper will listen for incoming ReAuth challenges.

```
luci-auth-ssh-helper -mode=daemon -port=10899
```

Then, use your SSH client and port-forward a port (here we use the same port
number for convenience) on your remote Linux machine to the helper’s port on the
local machine.

Note, you might need to update your SSH server config to allow port-forwarding
(if not enabled by default).

If you’re using the an OpenSSH client (e.g. built-in to Windows, or included in
Git-on-Windows):

```
ssh -R 10899:localhost:10899 [user@]remote_host
```

If you’re using PuTTY, set up port-forwarding on the "Connection \> SSH \>
Tunnels" page in the connection dialog (see screenshot). Remember to "Save" your
configuration in the "Session" page if you want to persist the configuration.

![](./images/gerrit_reauth_putty.png)

Inside your SSH session, set `SSH_AUTH_SOCK` to the forwarding port, then run
the ReAuth command.

```
export SSH_AUTH_SOCK=localhost:10899
git credential-luci reauth
```

Windows will prompt you to touch the security key. Touch the security to
complete ReAuth. If you see "ReAuth succeed", then it works.

For the first security key touch, there might be a delay before your security
key starts blinking. This is caused by `luci-auth-ssh-plugin` and
`luci-auth-fido2-plugin` bootstrapping.

You need to make sure `luci-auth-ssh-helper` is running on your local machine
when you want to perform ReAuth challenges over a SSH session. For convenience,
you can register it to start as a service on login.

### I’m using a Windows client, I want to remote desktop into Windows

Use the built-in Windows Remote Desktop Connection application (also known as
`mstsc`), make sure "WebAuthn (Windows Hello or security keys)" is enabled in
"Show Options \> Local Resources \> More…" (refer to screenshots below). Then
connect to the remote Windows machine as usual.

![](./images/gerrit_reauth_rdp1.png)

![](./images/gerrit_reauth_rdp2.png)

Then, in the remote desktop session, run the following command in command prompt
(CMD):

```shell
git credential-luci reauth
```

Windows will prompt you to touch the security key. Touch it to complete ReAuth.

If you see "ReAuth succeed", then it works\!

### None of the above

SSH / remote desktop workflows not listed above aren’t tested. We’re working on
adding instructions for more workflows.

If you have suggestions or feedback, please report to:
[https://issues.chromium.org/issues/new?component=1456702&template=2176581](https://issues.chromium.org/issues/new?component=1456702&template=2176581).

## Troubleshooting

Please set `LUCI_AUTH_DEBUG` environment variable to enable debug logs.

In Linux / Mac, run:

```
export LUCI_AUTH_DEBUG=1
```

In Windows (CMD), run:

```
set LUCI_AUTH_DEBUG=1
```

Then, retry the failed command (e.g. `git credential-luci reauth`).

If you run into issues, please report to
[https://issues.chromium.org/issues/new?component=1456702&template=2176581](https://issues.chromium.org/issues/new?component=1456702&template=2176581)

**Please be sure to include**:

- The debug logs produced by setting `LUCI_AUTH_DEBUG`
- The security key you're using (e.g. manufacturer, model, etc.)
- Whether the security key is registered as a FIDO2 or U2F key (see
  [Prerequisites](#prerequisites))
- The following environment variables: `SSH_AUTH_SOCK`, `SSH_CONNECTION` and
  `GOOGLE_AUTH_WEBAUTHN_PLUGIN`

Note, when sharing debug logs, please edit out the value after `Signature:`
field (if it's present) and any other values if you wish.

## FAQs

**ReAuth in `screen`, `tmux`, `shpool`, etc.**
You need to manually set `GOOGLE_AUTH_WEBAUTHN_PLUGIN` environment variable for
ReAuth to work. This is in addition to the instructions above.

If you're a Googler, follow
[the internal guide](go/gerrit-reauth#bookmark=id.gohr0ejjvi49).

Otherwise, set the environment variable depending on your situation:

* To ReAuth locally: `GOOGLE_AUTH_WEBAUTHN_PLUGIN=luci-auth-fido2-plugin`
* To ReAuth over SSH: `GOOGLE_AUTH_WEBAUTHN_PLUGIN=luci-auth-ssh-plugin`

Then run `git credential-luci reauth`.

**I accidentally shared the `Signature:` in the debug logs\!**

Do not worry too much if you share this. This can be used in a very small time
frame to exchange for a token that only lasts for 20 hours, and both the
exchange and any subsequent use of the token also requires your actual/regular
credentials in addition to the token. Furthermore, as of this writing, no
actions can be authorized with this token yet.

Of course, we do recommend avoiding sharing this as a general safety precaution.

**Can I use other forms of 2-Step Verification (2SV)?**

For ReAuth: No. You must use a physical security key. SMS, authenticator app,
passkeys won't satisfy ReAuth requirement (e.g. when uploading code, doing code
reviews).

You can still add and use other 2SV methods to sign into your Google account.

**What should I expect to see when ReAuth is required?**

ReAuth is required every 20 hours. When ReAuth is required you will see the
following error when performing Gerrit remote operations like uploading CLs:

```
ReAuth is required

If you are running this in a development environment, you can fix this by running:

git credential-luci reauth
```

You will need to run `git credential-luci reauth` every 20 hours to avoid or
resolve this issue. We recommend you ReAuth when you start your day.
