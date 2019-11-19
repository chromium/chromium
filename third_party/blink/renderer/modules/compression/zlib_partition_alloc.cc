#include "third_party/blink/renderer/modules/compression/zlib_partition_alloc.h"
#include "third_party/blink/renderer/platform/wtf/allocator/partitions.h"

namespace blink {

void ZlibPartitionAlloc::Configure(z_stream* stream) {
  stream->zalloc = Alloc;
  stream->zfree = Free;
}

void* ZlibPartitionAlloc::Alloc(void*, uint32_t items, uint32_t size) {
  // BufferMalloc is safer than FastMalloc when handling untrusted data.
  return WTF::Partitions::BufferMalloc(items * size, "zlib");
}

void ZlibPartitionAlloc::Free(void*, void* address) {
  WTF::Partitions::BufferFree(address);
}

}  // namespace blink
