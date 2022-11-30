// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CAPABILITIES_IN_MEMORY_VIDEO_DECODE_STATS_DB_IMPL_H_
#define MEDIA_CAPABILITIES_IN_MEMORY_VIDEO_DECODE_STATS_DB_IMPL_H_

#include <map>
#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "components/leveldb_proto/public/proto_database.h"
#include "media/base/media_export.h"
#include "media/base/video_codecs.h"
#include "media/capabilities/video_decode_stats_db.h"
#include "ui/gfx/geometry/size.h"

namespace media {

class VideoDecodeStatsDBProvider;

// The in-memory database disappears with profile shutdown to preserve the
// privacy of off-the-record (OTR) browsing profiles (Guest and Incognito). It
// also allows the MediaCapabilities API to behave the same both on and
// off-the-record which prevents sites from detecting when users are OTR modes.
// VideoDecodeStatsDBProvider gives incognito profiles a hook to read the stats
// of the of the originating profile. Guest profiles are conceptually a blank
// slate and will not have a "seed" DB.
class MEDIA_EXPORT InMemoryVideoDecodeStatsDBImpl : public VideoDecodeStatsDB {
 public:
  // |seed_db_provider| provides access to a seed (read-only) DB instance.
  // Callers must ensure the |seed_db_provider| outlives this factory and any
  // databases it creates via CreateDB(). |seed_db_provider| may be null when no
  // seed DB is available.
  explicit InMemoryVideoDecodeStatsDBImpl(
      VideoDecodeStatsDBProvider* seed_db_provider);

  InMemoryVideoDecodeStatsDBImpl(const InMemoryVideoDecodeStatsDBImpl&) =
      delete;
  InMemoryVideoDecodeStatsDBImpl& operator=(
      const InMemoryVideoDecodeStatsDBImpl&) = delete;

  ~InMemoryVideoDecodeStatsDBImpl() override;

  // Implement VideoDecodeStatsDB.
  void Initialize(InitializeCB init_cb) override;
  void AppendDecodeStats(const VideoDescKey& key,
                         const DecodeStatsEntry& entry,
                         AppendDecodeStatsCB append_done_cb) override;
  void GetDecodeStats(const VideoDescKey& key,
                      GetDecodeStatsCB get_stats_cb) override;
  void ClearStats(base::OnceClosure destroy_done_cb) override;

 private:
  // Called when the |seed_db_provider_| returns an initialized seed DB. Will
  // run |init_cb|, marking the completion of Initialize().
  void OnGotSeedDB(base::OnceCallback<void(bool)> init_cb,
                   VideoDecodeStatsDB* seed_db);

  // Passed as the callback for |OnGotDecodeStats| by |AppendDecodeStats| to
  // update the database once we've read the existing stats entry.
  void CompleteAppendWithSeedData(const VideoDescKey& key,
                                  const DecodeStatsEntry& entry,
                                  AppendDecodeStatsCB append_done_cb,
                                  bool read_success,
                                  std::unique_ptr<DecodeStatsEntry> seed_entry);

  // Called when GetDecodeStats() operation was performed. |get_stats_cb|
  // will be run with |success| and a |DecodeStatsEntry| created from
  // |stats_proto| or nullptr if no entry was found for the requested key.
  void OnGotSeedEntry(const VideoDescKey& key,
                      GetDecodeStatsCB get_stats_cb,
                      bool success,
                      std::unique_ptr<DecodeStatsEntry> seed_entry);

  // Indicates whether initialization is completed.
  bool db_init_ = false;

  // Lazily provides |seed_db_| from original profile. Owned by original profile
  // and may be null.
  raw_ptr<VideoDecodeStatsDBProvider> seed_db_provider_ = nullptr;

  // On-disk DB owned by the base profile for the off-the-record session. For
  // incognito sessions, this will contain the original profile's stats. For
  // guest sessions, this will be null (no notion of base profile). See
  // |in_memory_db_|.
  raw_ptr<VideoDecodeStatsDB> seed_db_ = nullptr;

  // In-memory DB, mapping VideoDescKey strings -> DecodeStatsEntries. This is
  // the primary storage (read and write) for this class. The |seed_db_| is
  // read-only, and  will only be queried when the |in_memory_db_| lacks an
  // entry for a given key.
  std::map<std::string, DecodeStatsEntry> in_memory_db_;

  // Ensures all access to class members come on the same sequence. API calls
  // and callbacks should occur on the same sequence used during construction.
  // LevelDB operations happen on a separate task runner, but all LevelDB
  // callbacks to this happen on the checked sequence.
  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<InMemoryVideoDecodeStatsDBImpl> weak_ptr_factory_{this};
};

}  // namespace media

#endif  // MEDIA_CAPABILITIES_IN_MEMORY_VIDEO_DECODE_STATS_DB_IMPL_H_
