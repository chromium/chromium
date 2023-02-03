# Overview

The quota system's primary role is to set and enforce limits on disk usage at
both the browser level, and at the origin level (see ./quota_settings.cc for
these limit values). The quota system manages disk usage only for certain web
platform storage APIs.

In order for a storage backend to integrate with the quota system, it must
implement the QuotaClient interface.

Most work on the quota system is currently done on the browser process' IO
thread.  There are plans for quota to be moved to [the Storage
Service](https://docs.google.com/document/d/1v83XKVxnasgf2uNfb_Uc-rfhDa3-ynNP23yU2DWqshI/),
which will run on its own process on desktop platforms.

# Key Components
## Interface
The quota system's interface is comprised of the following classes:

### QuotaManagerImpl
The "heart" of the quota system. This class lives on the browser
process' IO thread, but is primarily accessed through QuotaManagerProxy, which
handles thread hops. In the future, QuotaManagerProxy will turn into
mojom::QuotaManager, and the quota system will be accessed exclusively via mojo.

### QuotaClient
This interface must be implemented by any storage backend that wants to
integrate with the quota system. This is probably the most used interface from
outside of the quota system.

### PaddingKey
Helpers for computing quota usage for opaque resources. Features that store
opaque resources (e.g. Cache Storage) should use these helpers to avoid
leaking cross-origin information via the quota usage they report.

### SpecialStoragePolicy
Hook that allows browser features (currently Extensions and Chrome Apps) to
change an origin's quota.

## Implementation
The quota system's implementation is made up of the following components:

### UsageTracker, ClientUsageTracker
QuotaManagerImpl helpers that distribute tasks (e.g. measure an origin's quota
usage) across QuotaClient instances, and cache results as needed.

### QuotaDatabase
Stores persistent information in a per-StoragePartition SQLite database.
Currently stores a few bits of implementation details, and will likely be
expanded to cover Storage Buckets. The currently stored information is a usage
count, last-modified-time, and last-accessed-time for each origin (used to
implement LRU eviction on storage pressure, and Clear Site Data with a time
filter), and quota granted via the deprecated API
navigator.webkitPersistentStorage.requestQuota(1000, ...).

### QuotaTemporaryStorageEvictor
Handles eviction and records stats about eviction rounds.

### QuotaTask
Implementation detail of QuotaManagerImpl.

# Glossary

## Storage Pressure
A device is said to be under storage pressure when it is close to capacity.
Storage pressure is used to signal a couple of behaviors in the quota system:
 - Eviction
 - The QuotaChange event
 - Triggering storage pressure UI (implementation specific)

## Eviction
This is the process by which the quota system cleans up app's data as disk usage
gets close to the disk's capacity.

# Resources
 - [Chrome Web Storage and Quota Concepts](https://docs.google.com/document/d/19QemRTdIxYaJ4gkHYf2WWBNPbpuZQDNMpUVf8dQxj4U/)
   - In-depth description of the quota system that also explains related
     concepts and legacy APIs that left a mark on quota.
