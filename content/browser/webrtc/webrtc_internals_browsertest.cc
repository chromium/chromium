// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include "base/command_line.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/values_test_util.h"
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
  PeerConnectionEntry(int rid, int lid) : rid_(rid), lid_(lid) {}

  void AddEvent(const string& type, const string& value) {
    EventEntry entry = {type, value};
    events_.push_back(entry);
  }

  string getIdString() const {
    std::stringstream ss;
    ss << rid_ << "-" << lid_;
    return ss.str();
  }

  string getLogIdString() const {
    std::stringstream ss;
    ss << rid_ << "-" << lid_ << "-update-log";
    return ss.str();
  }

  string getAllUpdateString() const {
    std::stringstream ss;
    ss << "{rid:" << rid_ << ", lid:" << lid_ << ", log:[";
    for (size_t i = 0; i < events_.size(); ++i) {
      ss << "{type:'" << events_[i].type <<
          "', value:'" << events_[i].value << "'},";
    }
    ss << "]}";
    return ss.str();
  }

  int rid_;
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
  UserMediaRequestEntry(int rid,
                        int pid,
                        const std::string& origin,
                        const std::string& audio_constraints,
                        const std::string& video_constraints)
      : rid(rid),
        pid(pid),
        origin(origin),
        audio_constraints(audio_constraints),
        video_constraints(video_constraints) {}

  int rid;
  int pid;
  std::string origin;
  std::string audio_constraints;
  std::string video_constraints;
};

class WebRtcInternalsBrowserTest : public ContentBrowserTest {
 public:
  WebRtcInternalsBrowserTest() = default;
  ~WebRtcInternalsBrowserTest() override = default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch(switches::kUseFakeUIForMediaStream);
  }

  void SetUpOnMainThread() override {
    ASSERT_TRUE(base::CommandLine::ForCurrentProcess()->HasSwitch(
        switches::kUseFakeDeviceForMediaStream));
  }

 protected:
  bool ExecuteJavascript(const string& javascript) {
    return ExecJs(shell(), javascript);
  }

  void ExpectTitle(const std::string& expected_title) const {
    std::u16string expected_title16(base::ASCIIToUTF16(expected_title));
    TitleWatcher title_watcher(shell()->web_contents(), expected_title16);
    EXPECT_EQ(expected_title16, title_watcher.WaitAndGetTitle());
  }

  // Execute the javascript of addPeerConnection.
  void ExecuteAddPeerConnectionJs(const PeerConnectionEntry& pc) {
    std::stringstream ss;
    ss << "{rid:" << pc.rid_ << ", lid:" << pc.lid_ << ", pid:" << 0 << ", "
       << "url:'u', rtcConfiguration:'s', constraints:'c'}";
    ASSERT_TRUE(ExecuteJavascript(
        "cr.webUIListenerCallback('add-peer-connection', " + ss.str() + ");"));
  }

  // Execute the javascript of removePeerConnection.
  void ExecuteRemovePeerConnectionJs(const PeerConnectionEntry& pc) {
    std::stringstream ss;
    ss << "{rid:" << pc.rid_ << ", lid:" << pc.lid_ << "}";

    ASSERT_TRUE(ExecuteJavascript(
        "cr.webUIListenerCallback('remove-peer-connection', " + ss.str() +
        ");"));
  }

  // Execute the javascript of addMedia.
  void ExecuteAddMediaJs(const UserMediaRequestEntry& request) {
    std::stringstream ss;
    ss << "{rid:" << request.rid << ", pid:" << request.pid << ", origin:'"
       << request.origin << "', audio:'" << request.audio_constraints
       << "', video:'" << request.video_constraints << "'}";

    ASSERT_TRUE(ExecuteJavascript("cr.webUIListenerCallback('add-media', " +
                                  ss.str() + ");"));
  }

  // Execute the javascript of removeMediaForRenderer.
  void ExecuteRemoveMediaForRendererJs(int rid) {
    std::stringstream ss;
    ss << "{rid:" << rid << "}";
    ASSERT_TRUE(ExecuteJavascript(
        "cr.webUIListenerCallback('remove-media-for-renderer', " + ss.str() +
        ");"));
  }

  // Verifies that the DOM element with id |id| exists.
  void VerifyElementWithId(const string& id) {
    EXPECT_EQ(true, EvalJs(shell(),
                           "document.getElementById('" + id + "') != null;"));
  }

  // Verifies that the DOM element with id |id| does not exist.
  void VerifyNoElementWithId(const string& id) {
    EXPECT_EQ(true, EvalJs(shell(),
                           "document.getElementById('" + id + "') == null;"));
  }
  // Verifies that the JS object |graphViews| contains no keys starting with id
  // |id|.
  void VerifyNoGraphViewsWithId(const string& id) {
    EXPECT_EQ(true,
              EvalJs(shell(),
                     "Object.keys(graphViews).filter(key => key.startsWith('" +
                         id + "')).length === 0;"));
  }

  // Verifies the JS Array of userMediaRequests matches |requests|.
  void VerifyMediaRequest(const std::vector<UserMediaRequestEntry>& requests) {
    string json_requests =
        EvalJs(shell(), "JSON.stringify(userMediaRequests);").ExtractString();
    base::Value::List list_request = base::test::ParseJsonList(json_requests);

    EXPECT_EQ(requests.size(), list_request.size());

    for (size_t i = 0; i < requests.size(); ++i) {
      const base::Value& value = list_request[i];
      ASSERT_TRUE(value.is_dict());
      const base::Value::Dict& dict = value.GetDict();
      std::optional<int> rid = dict.FindInt("rid");
      std::optional<int> pid = dict.FindInt("pid");
      ASSERT_TRUE(rid);
      ASSERT_TRUE(pid);
      const std::string* origin = dict.FindString("origin");
      const std::string* audio = dict.FindString("audio");
      const std::string* video = dict.FindString("video");
      ASSERT_TRUE(origin);
      ASSERT_TRUE(audio);
      ASSERT_TRUE(video);
      EXPECT_EQ(requests[i].rid, *rid);
      EXPECT_EQ(requests[i].pid, *pid);
      EXPECT_EQ(requests[i].origin, *origin);
      EXPECT_EQ(requests[i].audio_constraints, *audio);
      EXPECT_EQ(requests[i].video_constraints, *video);
    }

    bool user_media_tab_existed =
        EvalJs(shell(), "document.querySelector('#user-media-tab-id') != null;")
            .ExtractBool();
    EXPECT_EQ(!requests.empty(), user_media_tab_existed);

    if (user_media_tab_existed) {
      int user_media_request_count =
          EvalJs(shell(),
                 "document.querySelector('#user-media-tab-id')"
                 "    .childNodes.length")
              .ExtractInt();
      // The list of childnodes includes the input field and its label.
      ASSERT_EQ(requests.size(),
                static_cast<size_t>(user_media_request_count) - 2);
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
      ss << "var row = document.getElementById('" << log_id << "').rows["
         << (i + 1)
         << "];"
            "var cell = row.lastChild;"
            "cell.firstChild.textContent;";
      EXPECT_EQ(pc.events_[i].type + pc.events_[i].value,
                EvalJs(shell(), ss.str()));
    }
  }

  // Executes the javascript of updatePeerConnection and verifies the result.
  void ExecuteAndVerifyUpdatePeerConnection(
      PeerConnectionEntry& pc, const string& type, const string& value) {
    pc.AddEvent(type, value);

    std::stringstream ss;
    ss << "{rid:" << pc.rid_ << ", lid:" << pc.lid_ << ", type:'" << type
       << "', value:'" << value << "'}";
    ASSERT_TRUE(ExecuteJavascript(
        "cr.webUIListenerCallback('update-peer-connection', " + ss.str() +
        ")"));

    VerifyPeerConnectionEntry(pc);
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

    EXPECT_EQ(name + ":" + value,
              EvalJs(shell(), "var row = document.getElementById('" + table_id +
                                  "-" + name +
                                  "');"
                                  "var name = row.cells[0].textContent;"
                                  "var value = row.cells[1].textContent;"
                                  "name + ':' + value"));
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
    EXPECT_EQ(true, EvalJs(shell(), "graphViews['" + pc_id + "-" + graph_id +
                                        "'] != null"));

    std::stringstream ss;
    ss << "var dp = peerConnectionDataStore['" << pc_id
       << "']"
          ".getDataSeries('"
       << graph_id << "').dataPoints_[" << index
       << "];"
          "dp.value.toString()";
    EXPECT_EQ(value, EvalJs(shell(), ss.str()));
  }
};

IN_PROC_BROWSER_TEST_F(WebRtcInternalsBrowserTest, AddAndRemovePeerConnection) {
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
  VerifyNoGraphViewsWithId(pc_1.getIdString());
  VerifyPeerConnectionEntry(pc_2);

  ExecuteRemovePeerConnectionJs(pc_2);
  VerifyNoElementWithId(pc_2.getIdString());
  VerifyNoGraphViewsWithId(pc_2.getIdString());
}

IN_PROC_BROWSER_TEST_F(WebRtcInternalsBrowserTest, UpdateAllPeerConnections) {
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
  EXPECT_TRUE(ExecuteJavascript(
      "cr.webUIListenerCallback('update-all-peer-connections', " + pc_array +
      ");"));
  VerifyPeerConnectionEntry(pc_0);
  VerifyPeerConnectionEntry(pc_1);
}

// Timing out on ARM linux bot: http://crbug.com/238490
// Disabling due to failure on Linux, Mac, Win: http://crbug.com/272413
// Sanity check of the page content under a real PeerConnection call.
IN_PROC_BROWSER_TEST_F(WebRtcInternalsBrowserTest,
                       DISABLED_WithRealPeerConnectionCall) {
  // Start a peerconnection call in the first window.
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url(embedded_test_server()->GetURL("/media/peerconnection-call.html"));
  EXPECT_TRUE(NavigateToURL(shell(), url));
  ASSERT_EQ(true, EvalJs(shell(), "call({video:true});"));
  ExpectTitle("OK");

  // Open webrtc-internals in the second window.
  GURL url2("chrome://webrtc-internals");
  Shell* shell2 = CreateBrowser();
  EXPECT_TRUE(NavigateToURL(shell2, url2));

  const int NUMBER_OF_PEER_CONNECTIONS = 2;

  // Verifies the number of peerconnections.
  EXPECT_EQ(NUMBER_OF_PEER_CONNECTIONS,
            EvalJs(shell2,
                   "document.querySelector('#peer-connections-list')"
                   "    .getElementsByTagName('li').length;"));

  // Verifies the the event tables.
  EXPECT_EQ(NUMBER_OF_PEER_CONNECTIONS,
            EvalJs(shell2,
                   "document.querySelector('#peer-connections-list')"
                   "    .getElementsByClassName('update-log-table').length;"));

  EXPECT_GT(
      EvalJs(shell2,
             "document.querySelector('#peer-connections-list')"
             "    .getElementsByClassName('update-log-table')[0].rows.length;"),
      1);

  EXPECT_GT(
      EvalJs(shell2,
             "document.querySelector('#peer-connections-list')"
             "    .getElementsByClassName('update-log-table')[1].rows.length;"),
      1);

  // Wait until the stats table containers are created.
  int count = 0;
  while (count != NUMBER_OF_PEER_CONNECTIONS) {
    count = EvalJs(shell2,
                   "document.querySelector('#peer-connections-list')"
                   "    .getElementsByClassName("
                   "    'stats-table-container').length;")
                .ExtractInt();
  }

  // Verifies each stats table having more than one rows.
  EXPECT_EQ(
      true,
      EvalJs(shell2,
             "var tableContainers = "
             "document.querySelector('#peer-connections-list')"
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
             "result;"));
}

IN_PROC_BROWSER_TEST_F(WebRtcInternalsBrowserTest, UpdateMedia) {
  GURL url("chrome://webrtc-internals");
  EXPECT_TRUE(NavigateToURL(shell(), url));

  UserMediaRequestEntry request1(1, 1, "origin", "ac", "vc");
  UserMediaRequestEntry request2(2, 2, "origin2", "ac2", "vc2");
  ExecuteAddMediaJs(request1);
  ExecuteAddMediaJs(request2);

  std::vector<UserMediaRequestEntry> list;
  list.push_back(request1);
  list.push_back(request2);
  VerifyMediaRequest(list);

  ExecuteRemoveMediaForRendererJs(1);
  list.erase(list.begin());
  VerifyMediaRequest(list);

  ExecuteRemoveMediaForRendererJs(2);
  list.erase(list.begin());
  VerifyMediaRequest(list);
}

}  // namespace content
