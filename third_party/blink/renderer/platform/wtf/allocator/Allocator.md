# Design Of PartitionAlloc In Blink

All objects in Blink are expected to be allocated with PartitionAlloc or Oilpan.

Blink uses different PartitionAlloc partitions, for different kinds of objects:

* LayoutObject partition: A partition to allocate `LayoutObject`s.  The
LayoutObject partition is a `ThreadUnsafePartitionAllocator`. Having a dedicated
partition for `LayoutObject`s improves cache locality and thus performance.

* Buffer partition: A partition to allocate objects that have a strong risk
that the length and/or the contents are exploited by user scripts. Specifically,
we allocate `Vector`s, `HashTable`s, and `String`s in the Buffer partition.

* ArrayBuffer partition: A partition to allocate `ArrayBufferContents`s.

* FastMalloc partition: A partition to allocate all other objects. Objects
marked with `USING_FAST_MALLOC` are allocated on the FastMalloc partition.

The Buffer partition and the FastMalloc partition have many buckets. They
support any arbitrary size of allocations but padding may be added to align the
allocation with the closest bucket size. The bucket sizes are chosen to keep the
worst-case memory overhead less than 10%.

## Security

We put `LayoutObject`s into a dedicated partition because `LayoutObject`s are
likely to be a source of use-after-free (UAF) vulnerabilities. Similarly, we put
`String`s, `Vector`s, et c. into the Buffer partition, and
`ArrayBufferContents`s into the ArrayBuffer partition, because malicious web
contents are likely to exploit the length field and/or contents of these
objects.

## Performance

PartitionAlloc doesn't acquire a lock when allocating on the LayoutObject
partition, because it's guaranteed that `LayoutObject`s are allocated only by
the main thread.

PartitionAlloc acquires a lock when allocating on the Buffer, ArrayBuffer, and
FastMalloc partitions.
