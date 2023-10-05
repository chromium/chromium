PRAGMA foreign_keys = OFF;

BEGIN TRANSACTION;

CREATE TABLE meta(key LONGVARCHAR NOT NULL UNIQUE PRIMARY KEY, value LONGVARCHAR);

INSERT INTO meta VALUES ('mmap_status', '-1');
INSERT INTO meta VALUES ('version', '6');
INSERT INTO meta VALUES ('last_compatible_version', '6');

CREATE TABLE interest_groups(
  expiration INTEGER NOT NULL,
  last_updated INTEGER NOT NULL,
  next_update_after INTEGER NOT NULL,
  owner TEXT NOT NULL,
  joining_origin TEXT NOT NULL,
  name TEXT NOT NULL,
  joining_url TEXT NOT NULL,
  bidding_url TEXT NOT NULL,
  bidding_wasm_helper_url TEXT NOT NULL,
  update_url TEXT NOT NULL,
  trusted_bidding_signals_url TEXT NOT NULL,
  trusted_bidding_signals_keys TEXT NOT NULL,
  user_bidding_signals TEXT,
  ads TEXT NOT NULL,
  ad_components TEXT NOT NULL,
  PRIMARY KEY(owner, name));

INSERT INTO interest_groups
VALUES
  (
    13293932603076872,
    13291340603081533,
    -9223372036854775808,
    'https://owner.example.com',
    'https://publisher.example.com',
    'groupNullUserBiddingSignals',
    'https://publisher.example.com/page1.html',
    'https://owner.example.com/bidder.js',
    '',
    'https://owner.example.com/update',
    'https://owner.example.com/signals',
    '["groupNullUserBiddingSignals"]',
    NULL,
    '[{"metadata":"[\"4\",\"5\",null,\"6\"]","url":"https://ads.example.com/1"}]',
    '');

INSERT INTO interest_groups
VALUES
  (
    13293932603076872,
    13291340603081533,
    -9223372036854775808,
    'https://owner.example.com',
    'https://publisher.example.com',
    'group1',
    'https://publisher.example.com/page1.html',
    'https://owner.example.com/bidder.js',
    '',
    'https://owner.example.com/update',
    'https://owner.example.com/signals',
    '["group1"]',
    '[["1","2"]]',
    '[{"metadata":"[\"4\",\"5\",null,\"6\"]","url":"https://ads.example.com/1"}]',
    '');

INSERT INTO interest_groups
VALUES
  (
    13293932603080090,
    13291340603089914,
    -9223372036854775808,
    'https://owner.example.com',
    'https://publisher.example.com',
    'group2',
    'https://publisher.example.com/page2.html',
    'https://owner.example.com/bidder.js',
    '',
    'https://owner.example.com/update',
    'https://owner.example.com/signals',
    '["group2"]',
    '[["1","3"]]',
    '[{"metadata":"[\"4\",\"5\",null,\"6\"]","url":"https://ads.example.com/1"}]',
    '');

INSERT INTO interest_groups
VALUES
  (
    13293932603052561,
    13291340603098283,
    -9223372036854775808,
    'https://owner.example.com',
    'https://publisher.example.com',
    'group3',
    'https://publisher.example.com/page3.html',
    'https://owner.example.com/bidder.js',
    '',
    'https://owner.example.com/update',
    'https://owner.example.com/signals',
    '["group3"]',
    '[["3","2"]]',
    '[{"metadata":"[\"4\",\"5\",null,\"6\"]","url":"https://ads.example.com/1"}]',
    '');

CREATE TABLE kanon(
  last_referenced_time INTEGER NOT NULL,
  type INTEGER NOT NULL,
  key TEXT NOT NULL,
  k_anon_count INTEGER NOT NULL,
  last_k_anon_updated_time INTEGER NOT NULL,
  last_reported_to_anon_server_time INTEGER NOT NULL,
  PRIMARY KEY(type, key));

INSERT INTO kanon
VALUES
  (
    13291340603098283,
    3,
    'https://ads.example.com/1',
    0,
    -9223372036854775808,
    -9223372036854775808);

INSERT INTO kanon
VALUES
  (
    13291340603089914,
    1,
    'https://owner.example.com/group2',
    0,
    -9223372036854775808,
    -9223372036854775808);

INSERT INTO kanon
VALUES
  (
    13291340603098283,
    2,
    'https://owner.example.com/update',
    0,
    -9223372036854775808,
    -9223372036854775808);

INSERT INTO kanon
VALUES
  (
    13291340603081533,
    1,
    'https://owner.example.com/group1',
    0,
    -9223372036854775808,
    -9223372036854775808);

INSERT INTO kanon
VALUES
  (
    13291340603098283,
    1,
    'https://owner.example.com/group3',
    0,
    -9223372036854775808,
    -9223372036854775808);

CREATE TABLE join_history(
  owner TEXT NOT NULL, name TEXT NOT NULL, join_time INTEGER NOT NULL,
  FOREIGN KEY(owner, name) REFERENCES interest_groups);

INSERT INTO join_history VALUES ('https://owner.example.com', 'group2', 13291340064197574);
INSERT INTO join_history VALUES ('https://owner.example.com', 'group1', 13291340064205914);
INSERT INTO join_history VALUES ('https://owner.example.com', 'group3', 13291340064214052);
INSERT INTO join_history VALUES ('https://owner.example.com', 'group3', 13291340515442940);
INSERT INTO join_history VALUES ('https://owner.example.com', 'group1', 13291340515453832);
INSERT INTO join_history VALUES ('https://owner.example.com', 'group2', 13291340515462085);
INSERT INTO join_history VALUES ('https://owner.example.com', 'group2', 13291340515470175);
INSERT INTO join_history VALUES ('https://owner.example.com', 'group1', 13291340515478159);
INSERT INTO join_history VALUES ('https://owner.example.com', 'group3', 13291340515486493);
INSERT INTO join_history VALUES ('https://owner.example.com', 'group2', 13291340603053960);
INSERT INTO join_history VALUES ('https://owner.example.com', 'group1', 13291340603072942);
INSERT INTO join_history VALUES ('https://owner.example.com', 'group1', 13291350603081533);
INSERT INTO join_history VALUES ('https://owner.example.com', 'group2', 13291350603089914);
INSERT INTO join_history VALUES ('https://owner.example.com', 'group3', 13291350603098283);

CREATE TABLE bid_history(
  owner TEXT NOT NULL, name TEXT NOT NULL, bid_time INTEGER NOT NULL,
  FOREIGN KEY(owner, name) REFERENCES interest_groups);

INSERT INTO bid_history VALUES ('https://owner.example.com', 'group3', 13291340515442940);
INSERT INTO bid_history VALUES ('https://owner.example.com', 'group1', 13291340515453832);
INSERT INTO bid_history VALUES ('https://owner.example.com', 'group2', 13291340515470175);
INSERT INTO bid_history VALUES ('https://owner.example.com', 'group1', 13291340515478159);
INSERT INTO bid_history VALUES ('https://owner.example.com', 'group3', 13291340515486493);
INSERT INTO bid_history VALUES ('https://owner.example.com', 'group2', 13291350603053960);
INSERT INTO bid_history VALUES ('https://owner.example.com', 'group3', 13291350603064576);
INSERT INTO bid_history VALUES ('https://owner.example.com', 'group1', 13291350603072942);
INSERT INTO bid_history VALUES ('https://owner.example.com', 'group1', 13291350603081533);
INSERT INTO bid_history VALUES ('https://owner.example.com', 'group2', 13291350603089914);
INSERT INTO bid_history VALUES ('https://owner.example.com', 'group3', 13291350603098283);

CREATE TABLE win_history(
  owner TEXT NOT NULL, name TEXT NOT NULL, win_time INTEGER NOT NULL, ad TEXT NOT NULL,
  FOREIGN KEY(owner, name) REFERENCES interest_groups);

CREATE
  INDEX interest_group_expiration
ON interest_groups(expiration DESC, owner, name);

CREATE
  INDEX interest_group_owner
ON interest_groups(owner, expiration DESC);

CREATE
  INDEX interest_group_joining_origin
ON interest_groups(joining_origin, expiration DESC, owner, name);

CREATE
  INDEX kanon_last_referenced_time
ON kanon(last_referenced_time DESC);

CREATE
  INDEX join_history_index
ON join_history(owner, name, join_time);

CREATE
  INDEX bid_history_index
ON bid_history(owner, name, bid_time);

CREATE
  INDEX win_history_index
ON win_history(owner, name, win_time DESC);

COMMIT;
