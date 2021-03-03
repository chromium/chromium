// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include "base/command_line.h"
#include "base/json/json_reader.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "base/values.h"
#include "build/build_config.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/shell/browser/shell.h"
#include "media/base/media_switches.h"
#include "net/test/embedded_test_server/embedded_test_server.h"

using std::string;
namespace content {

struct SsrcEntry {
  string GetSsrcAttributeString() const {
    std::stringstream ss;
    ss << "a=ssrc:" << id;
    std::map<string, string>::const_iterator iter;
    for (iter = properties.begin(); iter != properties.end(); ++iter) {
      ss << " " << iter->first << ":" << iter->second;
    }
    return ss.str();
  }

  string GetAsJSON() const {
    std::stringstream ss;
    ss << "{";
    std::map<string, string>::const_iterator iter;
    for (iter = properties.begin(); iter != properties.end(); ++iter) {
      if (iter != properties.begin())
        ss << ",";
      ss << "\"" << iter->first << "\":\"" << iter->second << "\"";
    }
    ss << "}";
    return ss.str();
  }

  string id;
  std::map<string, string> properties;
};

struct EventEntry {
  string type;
  string value;
};

struct StatsUnit {
  string GetString() const {
    std::stringstream ss;
    ss << "{timestamp:" << timestamp << ", values:[";
    std::map<string, string>::const_iterator iter;
    for (iter = values.begin(); iter != values.end(); ++iter) {
      ss << "'" << iter->first << "','" << iter->second << "',";
    }
    ss << "]}";
    return ss.str();
  }

  int64_t timestamp;
  std::map<string, string> values;
};

struct StatsEntry {
  string type;
  string id;
  StatsUnit stats;
};

typedef std::map<string, std::vector<string> > StatsMap;

class PeerConnectionEntry {
 public:
  PeerConnectionEntry(int pid, int lid) : pid_(pid), lid_(lid) {}

  void AddEvent(const string& type, const string& value) {
    EventEntry entry = {type, value};
    events_.push_back(entry);
  }

  string getIdString() const {
    std::stringstream ss;
    ss << pid_ << "-" << lid_;
    return ss.str();
  }

  string getLogIdString() const {
    std::stringstream ss;
    ss << pid_ << "-" << lid_ << "-update-log";
    return ss.str();
  }

  string getAllUpdateString() const {
    std::stringstream ss;
    ss << "{pid:" << pid_ << ", lid:" << lid_ << ", log:[";
    for (size_t i = 0; i < events_.size(); ++i) {
      ss << "{type:'" << events_[i].type <<
          "', value:'" << events_[i].value << "'},";
    }
    ss << "]}";
    return ss.str();
  }

  int pid_;
  int lid_;
  std::vector<EventEntry> events_;
  // This is a record of the history of stats value reported for each stats
  // report id (e.g. ssrc-1234) for each stats name (e.g. framerate).
  // It a 2-D map with each map entry is a vector of reported values.
  // It is used to verify the graph data series.
  std::map<string, StatsMap> stats_;
};

class UserMediaRequestEntry {
 public:
  UserMediaRequestEntry(int pid,
                        int rid,
                        const std::string& origin,
                        const std::string& audio_constraints,
                        const std::string& video_constraints)
      : pid(pid),
        rid(rid),
        origin(origin),
        audio_constraints(audio_constraints),
        video_constraints(video_constraints) {}

  int pid;
  int rid;
  std::string origin;
  std::string audio_constraints;
  std::string video_constraints;
};

static const int64_t FAKE_TIME_STAMP = 3600000;

#if defined(OS_WIN)
// All tests are flaky on Windows: crbug.com/277322.
#define MAYBE_WebRtcInternalsBrowserTest DISABLED_WebRtcInternalsBrowserTest
#else
#define MAYBE_WebRtcInternalsBrowserTest WebRtcInternalsBrowserTest
#endif

class MAYBE_WebRtcInternalsBrowserTest: public ContentBrowserTest {
 public:
  MAYBE_WebRtcInternalsBrowserTest() {}
  ~MAYBE_WebRtcInternalsBrowserTest() override {}

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch(switches::kUseFakeUIForMediaStream);
  }

  void SetUpOnMainThread() override {
    ASSERT_TRUE(base::CommandLine::ForCurrentProcess()->HasSwitch(
        switches::kUseFakeDeviceForMediaStream));
  }

 protected:
  bool ExecuteJavascript(const string& javascript) {
    return ExecuteScript(shell(), javascript);
  }

  void ExpectTitle(const std::string& expected_title) const {
    base::string16 expected_title16(base::ASCIIToUTF16(expected_title));
    TitleWatcher title_watcher(shell()->web_contents(), expected_title16);
    EXPECT_EQ(expected_title16, title_watcher.WaitAndGetTitle());
  }

  // Execute the javascript of addPeerConnection.
  void ExecuteAddPeerConnectionJs(const PeerConnectionEntry& pc) {
    std::stringstream ss;
    ss << "{pid:" << pc.pid_ <<", lid:" << pc.lid_ << ", " <<
           "url:'u', rtcConfiguration:'s', constraints:'c'}";
    ASSERT_TRUE(ExecuteJavascript("addPeerConnection(" + ss.str() + ");"));
  }

  // Execute the javascript of removePeerConnection.
  void ExecuteRemovePeerConnectionJs(const PeerConnectionEntry& pc) {
    std::stringstream ss;
    ss << "{pid:" << pc.pid_ <<", lid:" << pc.lid_ << "}";

    ASSERT_TRUE(ExecuteJavascript("removePeerConnection(" + ss.str() + ");"));
  }

  // Execute the javascript of addGetUserMedia.
  void ExecuteAddGetUserMediaJs(const UserMediaRequestEntry& request) {
    std::stringstream ss;
    ss << "{pid:" << request.pid << ", rid:" << request.rid << ", origin:'"
       << request.origin << "', audio:'" << request.audio_constraints
       << "', video:'" << request.video_constraints << "'}";

    ASSERT_TRUE(ExecuteJavascript("addGetUserMedia(" + ss.str() + ");"));
  }

  // Execute the javascript of removeGetUserMediaForRenderer.
  void ExecuteRemoveGetUserMediaForRendererJs(int rid) {
    std::stringstream ss;
    ss << "{rid:" << rid << "}";
    ASSERT_TRUE(
        ExecuteJavascript("removeGetUserMediaForRenderer(" + ss.str() + ");"));
  }

  // Verifies that the DOM element with id |id| exists.
  void VerifyElementWithId(const string& id) {
    bool result = false;
    ASSERT_TRUE(ExecuteScriptAndExtractBool(
        shell(),
        "window.domAutomationController.send($('" + id + "') != null);",
        &result));
    EXPECT_TRUE(result);
  }

  // Verifies that the DOM element with id |id| does not exist.
  void VerifyNoElementWithId(const string& id) {
    bool result = false;
    ASSERT_TRUE(ExecuteScriptAndExtractBool(
        shell(),
        "window.domAutomationController.send($('" + id + "') == null);",
        &result));
    EXPECT_TRUE(result);
  }

  // Verifies the JS Array of userMediaRequests matches |requests|.
  void VerifyUserMediaRequest(
      const std::vector<UserMediaRequestEntry>& requests) {
    string json_requests;
    ASSERT_TRUE(
        ExecuteScriptAndExtractString(shell(),
                                      "window.domAutomationController.send("
                                      "    JSON.stringify(userMediaRequests));",
                                      &json_requests));
    std::unique_ptr<base::Value> value_requests =
        base::JSONReader::ReadDeprecated(json_requests);

    EXPECT_EQ(base::Value::Type::LIST, value_requests->type());

    base::ListValue* list_request =
        static_cast<base::ListValue*>(value_requests.get());
    EXPECT_EQ(requests.size(), list_request->GetSize());

    for (size_t i = 0; i < requests.size(); ++i) {
      base::DictionaryValue* dict = nullptr;
      ASSERT_TRUE(list_request->GetDictionary(i, &dict));
      int pid, rid;
      std::string origin, audio, video;
      ASSERT_TRUE(dict->GetInteger("pid", &pid));
      ASSERT_TRUE(dict->GetInteger("rid", &rid));
      ASSERT_TRUE(dict->GetString("origin", &origin));
      ASSERT_TRUE(dict->GetString("audio", &audio));
      ASSERT_TRUE(dict->GetString("video", &video));
      EXPECT_EQ(requests[i].pid, pid);
      EXPECT_EQ(requests[i].rid, rid);
      EXPECT_EQ(requests[i].origin, origin);
      EXPECT_EQ(requests[i].audio_constraints, audio);
      EXPECT_EQ(requests[i].video_constraints, video);
    }

    bool user_media_tab_existed = false;
    ASSERT_TRUE(
        ExecuteScriptAndExtractBool(shell(),
                                    "window.domAutomationController.send("
                                    "    $('user-media-tab-id') != null);",
                                    &user_media_tab_existed));
    EXPECT_EQ(!requests.empty(), user_media_tab_existed);

    if (user_media_tab_existed) {
      int user_media_request_count = -1;
      ASSERT_TRUE(ExecuteScriptAndExtractInt(
          shell(),
          "window.domAutomationController.send("
          "    $('user-media-tab-id').childNodes.length);",
          &user_media_request_count));
      ASSERT_EQ(requests.size(), static_cast<size_t>(user_media_request_count));
    }
  }

  // Verifies that DOM for |pc| is correctly created with the right content.
  void VerifyPeerConnectionEntry(const PeerConnectionEntry& pc) {
    VerifyElementWithId(pc.getIdString());
    if (pc.events_.size() == 0)
      return;

    string log_id = pc.getLogIdString();
    VerifyElementWithId(log_id);
    string result;
    for (size_t i = 0; i < pc.events_.size(); ++i) {
      std::stringstream ss;
      ss << "var row = $('" << log_id << "').rows[" << (i + 1) << "];"
            "var cell = row.lastChild;"
            "window.domAutomationController.send(cell.firstChild.textContent);";
      ASSERT_TRUE(ExecuteScriptAndExtractString(shell(), ss.str(), &result));
      EXPECT_EQ(pc.events_[i].type + pc.events_[i].value, result);
    }
  }

  // Executes the javascript of updatePeerConnection and verifies the result.
  void ExecuteAndVerifyUpdatePeerConnection(
      PeerConnectionEntry& pc, const string& type, const string& value) {
    pc.AddEvent(type, value);

    std::stringstream ss;
    ss << "{pid:" << pc.pid_ <<", lid:" << pc.lid_ <<
         ", type:'" << type << "', value:'" << value << "'}";
    ASSERT_TRUE(ExecuteJavascript("updatePeerConnection(" + ss.str() + ")"));

    VerifyPeerConnectionEntry(pc);
  }

  // Execute addStats and verifies that the stats table has the right content.
  void ExecuteAndVerifyAddStats(
      PeerConnectionEntry& pc, const string& type, const string& id,
      StatsUnit& stats) {
    StatsEntry entry = {type, id, stats};

    // Adds each new value to the map of stats history.
    std::map<string, string>::iterator iter;
    for (iter = stats.values.begin(); iter != stats.values.end(); iter++) {
      pc.stats_[id][iter->first].push_back(iter->second);
    }
    std::stringstream ss;
    ss << "(() => {\n";
    ss << "  currentGetStatsMethod = OPTION_GETSTATS_LEGACY;\n";
    ss << "  addLegacyStats({pid:" << pc.pid_ << ", lid:" << pc.lid_
       << ", reports:[{id:'" << id << "', type:'" << type
       << "', stats:" << stats.GetString() << "}]});\n";
    ss << "})()";
    ASSERT_TRUE(ExecuteJavascript(ss.str()));
    VerifyStatsTable(pc, entry);
  }


  // Verifies that the stats table has the right content.
  void VerifyStatsTable(const PeerConnectionEntry& pc,
                        const StatsEntry& report) {
    string table_id =
        pc.getIdString() + "-table-" + report.id;
    VerifyElementWithId(table_id);

    std::map<string, string>::const_iterator iter;
    for (iter = report.stats.values.begin();
         iter != report.stats.values.end(); iter++) {
      VerifyStatsTableRow(table_id, iter->first, iter->second);
    }
  }

  // Verifies that the row named as |name| of the stats table |table_id| has
  // the correct content as |name| : |value|.
  void VerifyStatsTableRow(const string& table_id,
                           const string& name,
                           const string& value) {
    VerifyElementWithId(table_id + "-" + name);

    string result;
    ASSERT_TRUE(ExecuteScriptAndExtractString(
        shell(),
        "var row = $('" + table_id + "-" + name + "');"
        "var name = row.cells[0].textContent;"
        "var value = row.cells[1].textContent;"
        "window.domAutomationController.send(name + ':' + value)",
        &result));
    EXPECT_EQ(name + ":" + value, result);
  }

  // Verifies that the graph data series consistent with pc.stats_.
  void VerifyStatsGraph(const PeerConnectionEntry& pc) {
    std::map<string, StatsMap>::const_iterator stream_iter;
    for (stream_iter = pc.stats_.begin();
         stream_iter != pc.stats_.end(); stream_iter++) {
      StatsMap::const_iterator stats_iter;
      for (stats_iter = stream_iter->second.begin();
           stats_iter != stream_iter->second.end();
           stats_iter++) {
        string graph_id = stream_iter->first + "-" + stats_iter->first;
        for (size_t i = 0; i < stats_iter->second.size(); ++i) {
          float number;
          std::stringstream stream(stats_iter->second[i]);
          stream >> number;
          if (stream.fail())
            continue;
          VerifyGraphDataPoint(
              pc.getIdString(), graph_id, i, stats_iter->second[i]);
        }
      }
    }
  }

  // Verifies that the graph data point at index |index| has value |value|.
  void VerifyGraphDataPoint(const string& pc_id, const string& graph_id,
                            int index, const string& value) {
    bool result = false;
    ASSERT_TRUE(ExecuteScriptAndExtractBool(
        shell(),
        "window.domAutomationController.send("
           "graphViews['" + pc_id + "-" + graph_id + "'] != null)",
        &result));
    EXPECT_TRUE(result);

    std::stringstream ss;
    ss << "var dp = peerConnectionDataStore['" << pc_id << "']"
          ".getDataSeries('" << graph_id << "').dataPoints_[" << index << "];"
          "window.domAutomationController.send(dp.value.toString())";
    string actual_value;
    ASSERT_TRUE(
        ExecuteScriptAndExtractString(shell(), ss.str(), &actual_value));
    EXPECT_EQ(value, actual_value);
  }

  // Get the JSON string of the ssrc info from the page.
  string GetSsrcInfo(const string& ssrc_id) {
    string result;
    EXPECT_TRUE(ExecuteScriptAndExtractString(
        shell(),
        "window.domAutomationController.send(JSON.stringify("
           "ssrcInfoManager.streamInfoContainer_['" + ssrc_id + "']))",
        &result));
    return result;
  }

  int GetSsrcInfoBlockCount(Shell* shell) {
    int count = 0;
    EXPECT_TRUE(ExecuteScriptAndExtractInt(
        shell,
        "window.domAutomationController.send("
        "    document.getElementsByClassName("
        "        ssrcInfoManager.SSRC_INFO_BLOCK_CLASS).length);",
        &count));
    return count;
  }

  // Verifies |dump| contains |peer_connection_number| peer connection dumps,
  // each containing |update_number| updates and |stats_number| stats tables.
  void VerifyPageDumpStructure(base::Value* dump,
                               int peer_connection_number,
                               int update_number,
                               int stats_number) {
    EXPECT_NE((base::Value*)nullptr, dump);
    EXPECT_EQ(base::Value::Type::DICTIONARY, dump->type());

    base::DictionaryValue* dict_dump =
        static_cast<base::DictionaryValue*>(dump);
    EXPECT_EQ((size_t) peer_connection_number, dict_dump->size());

    base::DictionaryValue::Iterator it(*dict_dump);
    for (; !it.IsAtEnd(); it.Advance()) {
      base::Value* value = nullptr;
      dict_dump->Get(it.key(), &value);
      EXPECT_EQ(base::Value::Type::DICTIONARY, value->type());
      base::DictionaryValue* pc_dump =
          static_cast<base::DictionaryValue*>(value);
      EXPECT_TRUE(pc_dump->HasKey("updateLog"));
      EXPECT_TRUE(pc_dump->HasKey("stats"));

      // Verifies the number of updates.
      pc_dump->Get("updateLog", &value);
      EXPECT_EQ(base::Value::Type::LIST, value->type());
      base::ListValue* list = static_cast<base::ListValue*>(value);
      EXPECT_EQ((size_t) update_number, list->GetSize());

      // Verifies the number of stats tables.
      pc_dump->Get("stats", &value);
      EXPECT_EQ(base::Value::Type::DICTIONARY, value->type());
      base::DictionaryValue* dict = static_cast<base::DictionaryValue*>(value);
      EXPECT_EQ((size_t) stats_number, dict->size());
    }
  }

  // Verifies |dump| contains the correct statsTable and statsDataSeries for
  // |pc|.
  void VerifyStatsDump(base::Value* dump,
                       const PeerConnectionEntry& pc,
                       const string& report_type,
                       const string& report_id,
                       const StatsUnit& stats) {
    EXPECT_NE((base::Value*)nullptr, dump);
    EXPECT_EQ(base::Value::Type::DICTIONARY, dump->type());

    base::DictionaryValue* dict_dump =
        static_cast<base::DictionaryValue*>(dump);
    base::Value* value = nullptr;
    dict_dump->Get(pc.getIdString(), &value);
    base::DictionaryValue* pc_dump = static_cast<base::DictionaryValue*>(value);

    // Verifies there is one data series per stats name.
    value = nullptr;
    pc_dump->Get("stats", &value);
    EXPECT_EQ(base::Value::Type::DICTIONARY, value->type());

    base::DictionaryValue* dataSeries =
        static_cast<base::DictionaryValue*>(value);
    EXPECT_EQ(stats.values.size(), dataSeries->size());
  }
};

IN_PROC_BROWSER_TEST_F(MAYBE_WebRtcInternalsBrowserTest,
                       AddAndRemovePeerConnection) {
  GURL url("chrome://webrtc-internals");
  EXPECT_TRUE(NavigateToURL(shell(), url));

  // Add two PeerConnections and then remove them.
  PeerConnectionEntry pc_1(1, 0);
  ExecuteAddPeerConnectionJs(pc_1);
  VerifyPeerConnectionEntry(pc_1);

  PeerConnectionEntry pc_2(2, 1);
  ExecuteAddPeerConnectionJs(pc_2);
  VerifyPeerConnectionEntry(pc_2);

  ExecuteRemovePeerConnectionJs(pc_1);
  VerifyNoElementWithId(pc_1.getIdString());
  VerifyPeerConnectionEntry(pc_2);

  ExecuteRemovePeerConnectionJs(pc_2);
  VerifyNoElementWithId(pc_2.getIdString());
}

IN_PROC_BROWSER_TEST_F(MAYBE_WebRtcInternalsBrowserTest,
                       UpdateAllPeerConnections) {
  GURL url("chrome://webrtc-internals");
  EXPECT_TRUE(NavigateToURL(shell(), url));

  PeerConnectionEntry pc_0(1, 0);
  pc_0.AddEvent("e1", "v1");
  pc_0.AddEvent("e2", "v2");
  PeerConnectionEntry pc_1(1, 1);
  pc_1.AddEvent("e3", "v3");
  pc_1.AddEvent("e4", "v4");
  string pc_array = "[" + pc_0.getAllUpdateString() + ", " +
                          pc_1.getAllUpdateString() + "]";
  EXPECT_TRUE(ExecuteJavascript("updateAllPeerConnections(" + pc_array + ");"));
  VerifyPeerConnectionEntry(pc_0);
  VerifyPeerConnectionEntry(pc_1);
}

IN_PROC_BROWSER_TEST_F(MAYBE_WebRtcInternalsBrowserTest, UpdatePeerConnection) {
  GURL url("chrome://webrtc-internals");
  EXPECT_TRUE(NavigateToURL(shell(), url));

  // Add one PeerConnection and send one update.
  PeerConnectionEntry pc_1(1, 0);
  ExecuteAddPeerConnectionJs(pc_1);

  ExecuteAndVerifyUpdatePeerConnection(pc_1, "e1", "v1");

  // Add another PeerConnection and send two updates.
  PeerConnectionEntry pc_2(1, 1);
  ExecuteAddPeerConnectionJs(pc_2);

  SsrcEntry ssrc1, ssrc2;
  ssrc1.id = "ssrcid1";
  ssrc1.properties["msid"] = "mymsid";
  ssrc2.id = "ssrcid2";
  ssrc2.properties["label"] = "mylabel";
  ssrc2.properties["cname"] = "mycname";

  ExecuteAndVerifyUpdatePeerConnection(pc_2, "setRemoteDescription",
      ssrc1.GetSsrcAttributeString());

  ExecuteAndVerifyUpdatePeerConnection(pc_2, "createAnswerOnSuccess",
                                       ssrc2.GetSsrcAttributeString());
  ExecuteAndVerifyUpdatePeerConnection(pc_2, "setLocalDescription",
      ssrc2.GetSsrcAttributeString());

  EXPECT_EQ(ssrc1.GetAsJSON(), GetSsrcInfo(ssrc1.id));
  EXPECT_EQ(ssrc2.GetAsJSON(), GetSsrcInfo(ssrc2.id));

  StatsUnit stats = {FAKE_TIME_STAMP};
  stats.values["ssrc"] = ssrc1.id;
  ExecuteAndVerifyAddStats(pc_2, "ssrc", "dummyId", stats);
  EXPECT_GT(GetSsrcInfoBlockCount(shell()), 0);
}

// Tests that adding random named stats updates the dataSeries and graphs.
IN_PROC_BROWSER_TEST_F(MAYBE_WebRtcInternalsBrowserTest, AddStats) {
  GURL url("chrome://webrtc-internals");
  EXPECT_TRUE(NavigateToURL(shell(), url));

  PeerConnectionEntry pc(1, 0);
  ExecuteAddPeerConnectionJs(pc);

  const string type = "ssrc";
  const string id = "ssrc-1234";
  StatsUnit stats = {FAKE_TIME_STAMP};
  stats.values["trackId"] = "abcd";
  stats.values["bitrate"] = "2000";
  stats.values["framerate"] = "30";

  // Add new stats and verify the stats table and graphs.
  ExecuteAndVerifyAddStats(pc, type, id, stats);
  VerifyStatsGraph(pc);

  // Update existing stats and verify the stats table and graphs.
  stats.values["bitrate"] = "2001";
  stats.values["framerate"] = "31";
  ExecuteAndVerifyAddStats(pc, type, id, stats);
  VerifyStatsGraph(pc);
}

// Tests that the bandwidth estimation values are drawn on a single graph.
IN_PROC_BROWSER_TEST_F(MAYBE_WebRtcInternalsBrowserTest, BweCompoundGraph) {
  GURL url("chrome://webrtc-internals");
  EXPECT_TRUE(NavigateToURL(shell(), url));

  PeerConnectionEntry pc(1, 0);
  ExecuteAddPeerConnectionJs(pc);

  StatsUnit stats = {FAKE_TIME_STAMP};
  stats.values["googAvailableSendBandwidth"] = "1000000";
  stats.values["googTargetEncBitrate"] = "1000";
  stats.values["googActualEncBitrate"] = "1000000";
  stats.values["googRetransmitBitrate"] = "10";
  stats.values["googTransmitBitrate"] = "1000000";
  const string stats_type = "bwe";
  const string stats_id = "videobwe";
  ExecuteAndVerifyAddStats(pc, stats_type, stats_id, stats);

  string graph_id =
      pc.getIdString() + "-" + stats_id + "-bweCompound";
  bool result = false;
  // Verify that the bweCompound graph exists.
  ASSERT_TRUE(ExecuteScriptAndExtractBool(
        shell(),
        "window.domAutomationController.send("
        "   graphViews['" + graph_id + "'] != null)",
        &result));
  EXPECT_TRUE(result);

  // Verify that the bweCompound graph contains multiple dataSeries.
  int count = 0;
  ASSERT_TRUE(ExecuteScriptAndExtractInt(
        shell(),
        "window.domAutomationController.send("
        "   graphViews['" + graph_id + "'].getDataSeriesCount())",
        &count));
  EXPECT_EQ((int)stats.values.size(), count);
}

// Tests that the total packet/byte count is converted to count per second,
// and the converted data is drawn.
IN_PROC_BROWSER_TEST_F(MAYBE_WebRtcInternalsBrowserTest, ConvertedGraphs) {
  GURL url("chrome://webrtc-internals");
  EXPECT_TRUE(NavigateToURL(shell(), url));

  PeerConnectionEntry pc(1, 0);
  ExecuteAddPeerConnectionJs(pc);

  const string stats_type = "s";
  const string stats_id = "1";
  const int num_converted_stats = 4;
  const string stats_names[] =
      {"packetsSent", "bytesSent", "packetsReceived", "bytesReceived"};
  const string converted_names[] =
      {"packetsSentPerSecond", "bitsSentPerSecond",
       "packetsReceivedPerSecond", "bitsReceivedPerSecond"};
  const string first_value = "1000";
  const string second_value = "2000";
  const string converted_values[] = {"1000", "8000", "1000", "8000"};

  // Send the first data point.
  StatsUnit stats = {FAKE_TIME_STAMP};
  for (int i = 0; i < num_converted_stats; ++i)
    stats.values[stats_names[i]] = first_value;

  ExecuteAndVerifyAddStats(pc, stats_type, stats_id, stats);

  // Send the second data point at 1000ms after the first data point.
  stats.timestamp += 1000;
  for (int i = 0; i < num_converted_stats; ++i)
    stats.values[stats_names[i]] = second_value;
  ExecuteAndVerifyAddStats(pc, stats_type, stats_id, stats);

  // Verifies the graph data matches converted_values.
  for (int i = 0; i < num_converted_stats; ++i) {
    VerifyGraphDataPoint(pc.getIdString(), stats_id + "-" + converted_names[i],
                         1, converted_values[i]);
  }
}

// Timing out on ARM linux bot: http://crbug.com/238490
// Disabling due to failure on Linux, Mac, Win: http://crbug.com/272413
// Sanity check of the page content under a real PeerConnection call.
IN_PROC_BROWSER_TEST_F(MAYBE_WebRtcInternalsBrowserTest,
                       DISABLED_WithRealPeerConnectionCall) {
  // Start a peerconnection call in the first window.
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url(embedded_test_server()->GetURL("/media/peerconnection-call.html"));
  EXPECT_TRUE(NavigateToURL(shell(), url));
  ASSERT_TRUE(ExecuteJavascript("call({video:true});"));
  ExpectTitle("OK");

  // Open webrtc-internals in the second window.
  GURL url2("chrome://webrtc-internals");
  Shell* shell2 = CreateBrowser();
  EXPECT_TRUE(NavigateToURL(shell2, url2));

  const int NUMBER_OF_PEER_CONNECTIONS = 2;

  // Verifies the number of peerconnections.
  int count = 0;
  ASSERT_TRUE(ExecuteScriptAndExtractInt(
      shell2,
      "window.domAutomationController.send("
      "    $('peer-connections-list').getElementsByTagName('li').length);",
      &count));
  EXPECT_EQ(NUMBER_OF_PEER_CONNECTIONS, count);

  // Verifies the the event tables.
  ASSERT_TRUE(ExecuteScriptAndExtractInt(
      shell2,
      "window.domAutomationController.send($('peer-connections-list')"
      "    .getElementsByClassName('update-log-table').length);",
      &count));
  EXPECT_EQ(NUMBER_OF_PEER_CONNECTIONS, count);

  ASSERT_TRUE(ExecuteScriptAndExtractInt(
      shell2,
      "window.domAutomationController.send($('peer-connections-list')"
      "    .getElementsByClassName('update-log-table')[0].rows.length);",
      &count));
  EXPECT_GT(count, 1);

  ASSERT_TRUE(ExecuteScriptAndExtractInt(
      shell2,
      "window.domAutomationController.send($('peer-connections-list')"
      "    .getElementsByClassName('update-log-table')[1].rows.length);",
      &count));
  EXPECT_GT(count, 1);

  // Wait until the stats table containers are created.
  count = 0;
  while (count != NUMBER_OF_PEER_CONNECTIONS) {
    ASSERT_TRUE(ExecuteScriptAndExtractInt(
        shell2,
        "window.domAutomationController.send("
        "    $('peer-connections-list').getElementsByClassName("
        "        'stats-table-container').length);",
        &count));
  }

  // Verifies each stats table having more than one rows.
  bool result = false;
  ASSERT_TRUE(ExecuteScriptAndExtractBool(
      shell2,
      "var tableContainers = $('peer-connections-list')"
      "    .getElementsByClassName('stats-table-container');"
      "var result = true;"
      "for (var i = 0; i < tableContainers.length && result; ++i) {"
      "  var tables = tableContainers[i].getElementsByTagName('table');"
      "  for (var j = 0; j < tables.length && result; ++j) {"
      "    result = (tables[j].rows.length > 1);"
      "  }"
      "  if (!result) {"
      "    console.log(tableContainers[i].innerHTML);"
      "  }"
      "}"
      "window.domAutomationController.send(result);",
      &result));

  EXPECT_TRUE(result);

  count = GetSsrcInfoBlockCount(shell2);
  EXPECT_GT(count, 0);
}

IN_PROC_BROWSER_TEST_F(MAYBE_WebRtcInternalsBrowserTest, CreatePageDump) {
  GURL url("chrome://webrtc-internals");
  EXPECT_TRUE(NavigateToURL(shell(), url));

  PeerConnectionEntry pc_0(1, 0);
  pc_0.AddEvent("e1", "v1");
  pc_0.AddEvent("e2", "v2");
  PeerConnectionEntry pc_1(1, 1);
  pc_1.AddEvent("e3", "v3");
  pc_1.AddEvent("e4", "v4");
  string pc_array =
      "[" + pc_0.getAllUpdateString() + ", " + pc_1.getAllUpdateString() + "]";
  EXPECT_TRUE(ExecuteJavascript("updateAllPeerConnections(" + pc_array + ");"));

  // Verifies the peer connection data store can be created without stats.
  string dump_json;
  ASSERT_TRUE(ExecuteScriptAndExtractString(
      shell(),
      "window.domAutomationController.send("
      "    JSON.stringify(peerConnectionDataStore));",
      &dump_json));
  std::unique_ptr<base::Value> dump =
      base::JSONReader::ReadDeprecated(dump_json);
  VerifyPageDumpStructure(dump.get(),
                          2 /*peer_connection_number*/,
                          2 /*update_number*/,
                          0 /*stats_number*/);

  // Adds a stats report.
  const string type = "dummy";
  const string id = "1234";
  StatsUnit stats = { FAKE_TIME_STAMP };
  stats.values["bitrate"] = "2000";
  stats.values["framerate"] = "30";
  ExecuteAndVerifyAddStats(pc_0, type, id, stats);

  ASSERT_TRUE(ExecuteScriptAndExtractString(
      shell(),
      "window.domAutomationController.send("
      "    JSON.stringify(peerConnectionDataStore));",
      &dump_json));
  dump = base::JSONReader::ReadDeprecated(dump_json);
  VerifyStatsDump(dump.get(), pc_0, type, id, stats);
}

IN_PROC_BROWSER_TEST_F(MAYBE_WebRtcInternalsBrowserTest, UpdateGetUserMedia) {
  GURL url("chrome://webrtc-internals");
  EXPECT_TRUE(NavigateToURL(shell(), url));

  UserMediaRequestEntry request1(1, 1, "origin", "ac", "vc");
  UserMediaRequestEntry request2(2, 2, "origin2", "ac2", "vc2");
  ExecuteAddGetUserMediaJs(request1);
  ExecuteAddGetUserMediaJs(request2);

  std::vector<UserMediaRequestEntry> list;
  list.push_back(request1);
  list.push_back(request2);
  VerifyUserMediaRequest(list);

  ExecuteRemoveGetUserMediaForRendererJs(1);
  list.erase(list.begin());
  VerifyUserMediaRequest(list);

  ExecuteRemoveGetUserMediaForRendererJs(2);
  list.erase(list.begin());
  VerifyUserMediaRequest(list);
}

// Tests that the received propagation delta values are converted and drawn
// correctly.
IN_PROC_BROWSER_TEST_F(MAYBE_WebRtcInternalsBrowserTest,
                       ReceivedPropagationDelta) {
  GURL url("chrome://webrtc-internals");
  EXPECT_TRUE(NavigateToURL(shell(), url));

  PeerConnectionEntry pc(1, 0);
  ExecuteAddPeerConnectionJs(pc);

  StatsUnit stats = {FAKE_TIME_STAMP};
  stats.values["googReceivedPacketGroupArrivalTimeDebug"] =
      "[1000, 1100, 1200]";
  stats.values["googReceivedPacketGroupPropagationDeltaDebug"] =
      "[10, 20, 30]";
  const string stats_type = "bwe";
  const string stats_id = "videobwe";
  ExecuteAndVerifyAddStats(pc, stats_type, stats_id, stats);

  string graph_id = pc.getIdString() + "-" + stats_id +
      "-googReceivedPacketGroupPropagationDeltaDebug";
  string data_series_id =
      stats_id + "-googReceivedPacketGroupPropagationDeltaDebug";
  bool result = false;
  // Verify that the graph exists.
  ASSERT_TRUE(ExecuteScriptAndExtractBool(
      shell(),
      "window.domAutomationController.send("
      "   graphViews['" + graph_id + "'] != null)",
      &result));
  EXPECT_TRUE(result);

  // Verify that the graph contains multiple data points.
  int count = 0;
  ASSERT_TRUE(ExecuteScriptAndExtractInt(
      shell(),
      "window.domAutomationController.send("
      "   graphViews['" + graph_id + "'].getDataSeriesCount())",
      &count));
  EXPECT_EQ(1, count);
  ASSERT_TRUE(ExecuteScriptAndExtractInt(
      shell(),
      "window.domAutomationController.send("
      "   peerConnectionDataStore['" + pc.getIdString() + "']" +
      "       .getDataSeries('" + data_series_id + "').getCount())",
      &count));
  EXPECT_EQ(3, count);
}

}  // namespace content
