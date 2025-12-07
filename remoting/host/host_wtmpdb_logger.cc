// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "remoting/host/host_wtmpdb_logger.h"

#include <libgen.h>
#include <pty.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <unistd.h>

#include <string>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/strings/string_number_conversions.h"
#include "remoting/base/logging.h"
#include "remoting/host/host_status_monitor.h"
#include "sql/database.h"
#include "sql/statement.h"

namespace remoting {

namespace {

// Name to pass to wtmpdb as host.
constexpr char kApplicationName[] = "chromoting";
constexpr base::FilePath::CharType kDbPath[] =
    FILE_PATH_LITERAL("/var/lib/wtmpdb/wtmp.db");

constexpr int kUserProcess = 3;

uint64_t GetCurrentTimeMicros() {
  timeval now;
  gettimeofday(&now, nullptr);
  return now.tv_sec * ((uint64_t)1000000ULL) + now.tv_usec;
}

}  // namespace

HostWtmpdbLogger::HostWtmpdbLogger(scoped_refptr<HostStatusMonitor> monitor)
    : monitor_(monitor) {
  monitor_->AddStatusObserver(this);
}

HostWtmpdbLogger::~HostWtmpdbLogger() {
  monitor_->RemoveStatusObserver(this);
}

void HostWtmpdbLogger::OnClientConnected(const std::string& signaling_id) {
  int pty, replica_pty;
  if (openpty(&pty, &replica_pty, nullptr, nullptr, nullptr)) {
    PLOG(ERROR) << "Failed to open pty for wtmpdb logging";
    return;
  }
  close(replica_pty);

  sql::Database db(sql::DatabaseOptions{}, sql::Database::Tag("Chromoting"));
  base::FilePath file_path(kDbPath);
  base::CreateDirectory(file_path.DirName());
  if (!db.Open(file_path)) {
    PLOG(ERROR) << "Failed to open wtmpdb";
    close(pty);
    return;
  }

  constexpr base::cstring_view sql_table(
      "CREATE TABLE IF NOT EXISTS wtmp(ID INTEGER PRIMARY KEY, "
      "Type INTEGER, User TEXT NOT NULL, Login INTEGER, "
      "Logout INTEGER, TTY TEXT, RemoteHost TEXT, Service TEXT) STRICT;");

  if (!db.Execute(sql_table)) {
    PLOG(ERROR) << "Failed to create wtmp table";
    close(pty);
    return;
  }

  constexpr base::cstring_view sql_insert(
      "INSERT INTO wtmp (Type,User,Login,TTY,RemoteHost,Service) "
      "VALUES(?,?,?,?,?,?);");

  sql::Statement statement(db.GetCachedStatement(SQL_FROM_HERE, sql_insert));
  if (!statement.is_valid()) {
    PLOG(ERROR) << "Failed to prepare wtmpdb login query";
    close(pty);
    return;
  }

  statement.BindInt(/*param_index=*/0, /*Type=*/kUserProcess);
  statement.BindString(/*param_index=*/1, /*User=*/kApplicationName);
  statement.BindInt64(/*param_index=*/2, /*Login=*/GetCurrentTimeMicros());
  statement.BindString(/*param_index=*/3, /*TTY=*/base::NumberToString(pty));
  statement.BindString(/*param_index=*/4, /*RemoteHost=*/kApplicationName);
  statement.BindString(/*param_index=*/5, /*Service=*/"chrome-remote-desktop");

  if (!statement.Run()) {
    PLOG(ERROR) << "Failed to insert wtmpdb entry";
    close(pty);
    return;
  }

  session_.emplace(signaling_id, ConnectionInfo{pty, db.GetLastInsertRowId()});
}

void HostWtmpdbLogger::OnClientDisconnected(const std::string& signaling_id) {
  auto sess_iter = session_.find(signaling_id);
  if (sess_iter == session_.end()) {
    return;
  }
  sql::Database db(sql::DatabaseOptions{}, sql::Database::Tag("Chromoting"));
  base::FilePath file_path(kDbPath);
  if (!db.Open(file_path)) {
    PLOG(ERROR) << "Failed to open wtmp.db";
    close(sess_iter->second.pty_id);
    session_.erase(signaling_id);
    return;
  }

  constexpr base::cstring_view sql("UPDATE wtmp SET Logout = ? WHERE ID = ?");

  sql::Statement statement(db.GetCachedStatement(SQL_FROM_HERE, sql));
  if (!statement.is_valid()) {
    PLOG(ERROR) << "Failed to prepare wtmpdb logout query";
    close(sess_iter->second.pty_id);
    session_.erase(signaling_id);
    return;
  }
  statement.BindInt64(/*param_index=*/0, /*Logout=*/GetCurrentTimeMicros());
  statement.BindInt64(/*param_index=*/1, /*ID=*/sess_iter->second.wtmpdb_id);

  if (!statement.Run()) {
    PLOG(ERROR) << "Failed to update wtmpdb entry";
  }

  close(sess_iter->second.pty_id);
  session_.erase(signaling_id);
}

}  // namespace remoting
