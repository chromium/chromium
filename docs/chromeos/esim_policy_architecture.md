# ChromeOS eSIM Policy Architecture

## Background

The eSIM architecture on ChromeOS, particularly when it comes to enterprise
policy, is complicated and a holistic understanding is necessary in order to be
capable of reasoning about the behavior or issues that are observed. This
document will cover the requirements and assumptions ChromeOS maintains in
regards to eSIM installation on enterprise devices, the flow of installing eSIM
profiles itself, and the unintuitive “side-effects” of these
requirements/assumptions and flow.

## What’s What

**DPanel** is a user interface provided by the Commercial team that enterprise
administrators use to configure policies.

**DMServer** is the enterprise policy backend that is responsible for sending
the policies configured via DPanel to devices, and collecting information from
devices to be made available to administrators.

**Managed cellular networks** are configured via policy to install an eSIM profile.

**EID** is the unique identifier for the eSIM hardware of a ChromeOS (or any)
device. Typically, when an eSIM profile is configured by a carrier it is
associated with an EID, thus allowing only that device to be capable of
installing the profile.

**ICCID** is the unique identifier of an eSIM profile. This identifier is not
known to a device until the device has installed the profile.

**Activation codes** are strings that contain metadata that can be used to
discover eSIM profiles that are available for installation, or to retrieve all
of the information necessary to install a particular profile.

## The Requirements/Assumptions

The installation flow will appear too complicated with too many moving parts
without discussing the requirements and assumptions made by ChromeOS. The
requirements/assumptions are:

**All eSIM profiles on a device will persist through a powerwash.** Recovering
an eSIM profile after it has been erased is much more difficult than recovering
a Wi-Fi network; we require users to explicitly remove eSIM profiles.
Administrators are capable of removing all eSIM profiles with a single action.

**Each valid managed cellular network should result in one and only one eSIM
profile being installed on the device.** A policy configured by an administrator
will typically be pushed to many different devices. To make policy behavior
predictable ChromeOS enforces that a single policy should install a single
profile; if a second profile is desired then a second managed cellular network
should be configured.

**Enterprise enrolled devices should correctly identify eSIM profiles that were
installed via policy, even after a powerwash.** Administrators will often
powerwash and re-enroll devices. Since eSIM profiles persist through
powerwashing, and since each managed cellular network should result in one and
only one eSIM profile being installed, all profiles installed via policy before
a powerwash need to continue to be mapped to the corresponding policy.

**Administrators should be able to configure multiple managed cellular networks,
and these different networks should be able to use the same activation code.**
An activation code can correspond to 0 or more profiles and we do not want to
wrongly prevent enterprises from installing their purchased profiles.

## The Flow

The steps described below omit details that are not relevant for understanding
the end-to-end flow.

1. Enterprise administrator configures a cellular network using DPanel. The
   administrator provides an activation code that can be used by all devices
   receiving the policy.

    Note: All devices that receive this policy will attempt to install an eSIM
    profile using the provided activation code. If the activation code is not
    expected to work for the device for any reason the device will fail the
    installation.

2. DMServer sends the policy to all devices in the corresponding organizational
   unit (OU). An OU is a single node in the hierarchy used by administrators to
   control which devices get which policies. Before sending the policy to a
   device, DMServer may insert metadata that is relevant to the particular
   device.

3. ChromeOS receives the policy from DMServer and attempts to identify any newly
   configured managed cellular networks. ChromeOS will attempt to identify
   whether an eSIM profile should be installed for each network by checking the
   following:
  - Whether DMServer added metadata to the policy that indicates an eSIM
     profile has already been installed
  - Whether the policy appears to match an installed eSIM profile

4. ChromeOS iterates over the managed cellular networks that it needs to install
   an eSIM profile for and adds an installation request to a queue. As each
   request is completed, ChromeOS persists metadata about the profile to disk.

    ChromeOS will only persist metadata for eSIM profiles that have been
    successfully installed for a managed cellular network or when a managed
    cellular network is mapped to an eSIM profile that was previously installed,
    e.g.  ChromeOS detects after a powerwash that an existing eSIM profile was
    installed via policy.

    The metadata that is persisted will consist of the name of the managed
    cellular network, the activation code of the managed cellular network, and
    the ICCID of the eSIM profile that had been installed for the network.

5. ChromeOS uploads the metadata persisted to disk about eSIM profiles installed
   for cellular network policies to DMServer. During this upload, ChromeOS can
   indicate to DMServer that the existing metadata should be cleared. This is
   the case when an administrator uses the “reset eSIM” functionality provided
   by DPanel.

6. DMServer persists the metadata uploaded by ChromeOS devices and includes this
   metadata each time it sends a policy to the device in the future. To be
   clear: this metadata is the same metadata mentioned in Step #2.

## Resulting Behaviors

Now that we have an understanding of the different players and the flow we can
dive into the different resulting behaviors that can be observed.

### eSIM Profile Installed via Policy Appears Unmanaged

If a policy that resulted in an eSIM profile being installed is removed from
DPanel *without having removed the eSIM profiles on the device*, the eSIM
profile will no longer appear to have been installed via policy. The user will
be able to remove this profile via the UI.

### DMServer Metadata

The DMServer metadata is uploaded one of three ways.

1. ChromeOS uploads a flag indicating that DMServer should clear its existing
   metadata. ChromeOS may include new metadata to persist, but it is not
   required.  This is the operation performed when an administrator performs the
   “reset eSIM” command on the device.

2. ChromeOS uploads metadata containing information on one or more eSIM profiles
   that were installed via policy. DMServer overwrites the existing metadata
   with this latest state. This will only happen if the metadata has changed,
   such as from a profile being added or removed.

    Note: ChromeOS persists the last uploaded metadata locally and will only
    upload metadata to DMServer if it has changed.

3. ChromeOS uploads metadata containing information on zero eSIM profiles. This
   is the operation performed after an enterprise enrolled device is powerwashed
   and re-enrolled, or when ChromeOS no longer has any eSIM profiles that were
   installed via policy (see eSIM Profile Installed via Policy Appears
   Unmanaged).

    DMServer ignores this operation since it would effectively clear its
    metadata without ChromeOS explicitly requesting to do so.

**Behavior:** DMServer will continue to persist metadata about an eSIM profile
installed via policy even after it has been removed IFF it was the only eSIM
profile installed via policy and it was not removed by the administrator
performing the “reset eSIM” command.

**Impact:** Managed cellular networks may not result in a new eSIM profile being
installed if the managed cellular network has the same name and activation code
as a previous managed cellular network.
