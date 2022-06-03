#!/bin/sh
# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#
# The purpose of this script is to set up all the necessary magic to
# pipe network traffic through a user-space process. That user-space
# process can then delay, reorder and drop packets as it pleases to
# emulate various network environments.
#
# The script currently assumes that you communicate with your cast streaming
# receiver through eth1. After running "shadow.sh start", your network will
# look something like this:
#
#              +--------------------------------------------------+
#              |            Your linux machine                    |
#              | +---------------+                                |
# cast         | |shadowbr bridge|               +-------------+  |
# streaming <--+-+---> eth1      |               |routing table|  |
# receiver     | |     tap2  <---+-> tap_proxy <-+-> tap1      |  |
#              | |  +->veth      |               |   eth0 <----+--+->internet
#              | +--+------------+               |   lo        |  |
#              |    |                            +-------------+  |
#              |    |      +------------------+       ^           |
#              |    |      |shadow container  |       |           |
#              |    +------+-->veth           |     chrome        |
#              |           | netload.py server|  netload.py client|
#              |           +------------------+                   |
#              +--------------------------------------------------+
#
# The result should be that all traffic to/from the cast streaming receiver
# will go through tap_proxy. All traffic to/from the shadow container
# will also go through the tap_proxy. (A container is kind of like a
# virtual machine, but more lightweight.) Running "shadow.sh start" does
# not start the tap_proxy, so you'll have to start it manually with
# the command "tap_proxy tap1 tap2 <network_profile>" where
# <network_profile> is one of "perfect", "good", "wifi", "bad" or "evil".
#
# While testing mirroring, we can now generate TCP traffic through
# the tap proxy by talking to the netload server inside the "shadow"
# container by using the following command:
#
# $ netload.py upload IP PORT
#
# The IP and PORT are printed out by this script when you run
# "shadow.sh start", but will generally be the *.*.*.253 address
# of the eth1 network, so hopefully that's not already taken...

set -x

DEV=eth1
TAP1=tap1
TAP2=tap2

IP="$(ifconfig $DEV | sed -n 's@.*inet addr:\([^ ]*\).*@\1@gp')"
MASK="$(ifconfig $DEV | sed -n 's@.*Mask:\([^ ]*\).*@\1@gp')"
BCAST="$(ifconfig $DEV | sed -n 's@.*Bcast:\([^ ]*\).*@\1@gp')"
NET=$(route -n | grep $DEV | head -1 | awk '{print $1}')
DIR=$(dirname "$0")

case "$MASK" in
  255.255.255.0) MASK_BITS=24 ;;
  255.255.0.0) MASK_BITS=16 ;;
  255.0.0.0) MASK_BITS=8 ;;
  *)
    echo "Unknown network mask"
    exit 1
  ;;
esac

SHADOWIP="$(echo $IP | sed 's@[^.]*$@@g')253"
SHADOWCONF="/tmp/shadowconf.$$"
cat <<EOF >$SHADOWCONF
lxc.utsname = shadow
lxc.network.type = veth
lxc.network.link = shadowbr
lxc.network.flags = up
lxc.network.ipv4 = $SHADOWIP/$MASK_BITS
lxc.network.ipv4.gateway = $IP
lxc.kmsg = 0
EOF

trap "rm $SHADOWCONF" SIGINT SIGTERM EXIT
LXC_COMMON="-n shadow -f $SHADOWCONF"

case "$1" in
  start)
    openvpn --mktun --dev $TAP1
    openvpn --mktun --dev $TAP2
    ifconfig $TAP1 $IP netmask $MASK broadcast $BCAST up
    ifconfig $TAP2 up
    route add -net $NET netmask $MASK $TAP1
    brctl addbr shadowbr
    brctl addif shadowbr $TAP2 $DEV
    ifconfig shadowbr up
    lxc-create $LXC_COMMON
    lxc-execute $LXC_COMMON -- \
       "$DIRNAME/netload.py listen 9999" >/dev/null </dev/null 2>&1 &
    echo "Now run: tap_proxy $TAP1 $TAP2 wifi"
    echo "Data sink/source is available on $SHADOWIP 9999"
  ;;

  stop)
    lxc-kill -n shadow
    sleep 1
    lxc-destroy $LXC_COMMON
    ifconfig $TAP1 down
    ifconfig $TAP2 down
    ifconfig shadowbr down
    brctl delbr shadowbr
    openvpn --rmtun --dev $TAP1
    openvpn --rmtun --dev $TAP2
  ;;

  *)
    echo "$0 start/stop"
    echo "Read $0 for more information."
  ;;
esac


