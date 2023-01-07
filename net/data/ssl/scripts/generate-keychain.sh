#!/bin/sh

# Copyright 2016 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.


set -e -x

SECURITY=/usr/bin/security

KEYCHAIN="$1"
shift
# security create-keychain will interpret a non-absolute path relative to the
# keychain directory rather than the current directory, and OSX doesn't have a
# realpath command. Be lazy and make the user pass in an absolute path.
if [ `echo "$KEYCHAIN" | cut -c1` != '/' ]; then
  echo keychain path must be absolute
  exit 1
fi

PASSWORD=aoeu


# create-keychain modifes the global keychain search list, save it first.
# (or does it?)
SAVED_KEYCHAIN_LIST=`$SECURITY list -d user`
echo "Saved user keychain list:"
echo "$SAVED_KEYCHAIN_LIST"
echo


$SECURITY create-keychain -p "$PASSWORD" "$KEYCHAIN"

trusted=0

for cert in "$@"; do
  if [ "$cert" = "--trusted" ]; then
    trusted=1
    continue
  fi
  if [ "$cert" = "--untrusted" ]; then
    trusted=0
    continue
  fi

  # security tool only accepts DER. If input is a PEM, convert it.
  if grep -- "-----BEGIN CERTIFICATE-----" "$cert" ; then
    tmpcert="${cert}.der.tmp"
    openssl x509 -inform PEM -in "$cert" -outform DER -out "$tmpcert"
    cert="$tmpcert"
  fi

  if [ $trusted = 1 ]; then
    $SECURITY add-trusted-cert -r trustAsRoot -k "$KEYCHAIN" "$cert"
  else
    $SECURITY add-certificates -k "$KEYCHAIN" "$cert"
  fi
done



#TODO: Would be good to restore the keychain search list on failure too.

echo "pre-restore user keychain list:"
$SECURITY list -d user

# restore the original keychain search list
/bin/echo -n "${SAVED_KEYCHAIN_LIST}" | xargs $SECURITY list -d user -s

echo "Restored user keychain list:"
$SECURITY list -d user
echo
