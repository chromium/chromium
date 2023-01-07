# Working remotely with Android

[TOC]


## Introduction

If you want to work remotely from your laptop with an Android device attached to
it, while keeping an ssh connection to a remote desktop machine where you have
your build environment setup, you will have to use one of the two alternatives
listed below.

## Option 1: SSHFS - Mounting the out/Debug directory

### On your laptop

You have to have an Android device attached to it.

```shell
# Install sshfs

laptop$ sudo apt-get install sshfs

# Mount the chrome source from your remote host machine into your local laptop.

laptop$ mkdir ~/chrome_sshfs
laptop$ sshfs your.host.machine:/usr/local/code/chrome/src ./chrome_sshfs

# Setup environment.

laptop$ cd chrome_sshfs
laptop$ third_party/android_sdk/public/platform-tools/adb devices

# Run tests.

laptop$ out/Default/bin/run_$YOUR_TEST
```

*** note
This is assuming you have the exact same linux version on your host machine and
in your laptop.
***

But if you have different versions, lets say, Ubuntu Lucid on your laptop, and the newer Ubuntu Precise on your host machine, some binaries compiled on the host will not work on your laptop.
In this case you will have to recompile these binaries in your laptop:

```shell
# May need to install dependencies on your laptop.

laptop$ sudo ./build/install-build-deps.sh --android

# Rebuild the needed binaries on your laptop.

laptop$ ninja -C out/Debug md5sum host_forwarder
```

## Option 2: SSH Tunneling

Copy /tools/android/adb_remote_setup.sh to your laptop, then run it.
adb_remote_setup.sh updates itself, so you only need to copy it once.

```shell
laptop$ curl -sSf "https://chromium.googlesource.com/chromium/src.git/+/main/tools/android/adb_remote_setup.sh?format=TEXT" | base64 --decode > adb_remote_setup.sh
laptop$ chmod +x adb_remote_setup.sh
laptop$ ./adb_remote_setup.sh <desktop_hostname> <path_to_adb_on_desktop>
```

### On your host machine

```shell
desktop$ out/Default/bin/run_$YOUR_TEST
```
