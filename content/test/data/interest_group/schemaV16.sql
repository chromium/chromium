PRAGMA foreign_keys=OFF;
BEGIN TRANSACTION;
CREATE TABLE meta(key LONGVARCHAR NOT NULL UNIQUE PRIMARY KEY, value LONGVARCHAR);
INSERT INTO meta VALUES('mmap_status','-1');
INSERT INTO meta VALUES('last_compatible_version','6');
INSERT INTO meta VALUES('version','16');
CREATE TABLE win_history(
  owner TEXT NOT NULL, name TEXT NOT NULL, win_time INTEGER NOT NULL, ad TEXT NOT NULL,
  FOREIGN KEY(owner, name) REFERENCES interest_groups);
CREATE TABLE IF NOT EXISTS "join_history"(owner TEXT NOT NULL,name TEXT NOT NULL,join_time INTEGER NOT NULL,count INTEGER NOT NULL,PRIMARY KEY(owner, name, join_time) FOREIGN KEY(owner,name) REFERENCES interest_groups);
INSERT INTO join_history VALUES('https://owner.example.com','group1',13291257600000000,4);
INSERT INTO join_history VALUES('https://owner.example.com','group1',13291344000000000,1);
INSERT INTO join_history VALUES('https://owner.example.com','group2',13291257600000000,4);
INSERT INTO join_history VALUES('https://owner.example.com','group2',13291344000000000,1);
INSERT INTO join_history VALUES('https://owner.example.com','group3',13291257600000000,3);
INSERT INTO join_history VALUES('https://owner.example.com','group3',13291344000000000,1);
CREATE TABLE IF NOT EXISTS "bid_history"(owner TEXT NOT NULL,name TEXT NOT NULL,bid_time INTEGER NOT NULL,count INTEGER NOT NULL,PRIMARY KEY(owner, name, bid_time) FOREIGN KEY(owner,name) REFERENCES interest_groups);
INSERT INTO bid_history VALUES('https://owner.example.com','group1',13291257600000000,2);
INSERT INTO bid_history VALUES('https://owner.example.com','group1',13291344000000000,2);
INSERT INTO bid_history VALUES('https://owner.example.com','group2',13291257600000000,1);
INSERT INTO bid_history VALUES('https://owner.example.com','group2',13291344000000000,2);
INSERT INTO bid_history VALUES('https://owner.example.com','group3',13291257600000000,2);
INSERT INTO bid_history VALUES('https://owner.example.com','group3',13291344000000000,2);
CREATE TABLE k_anon(last_referenced_time INTEGER NOT NULL,key TEXT NOT NULL,is_k_anon INTEGER NOT NULL,last_k_anon_updated_time INTEGER NOT NULL,last_reported_to_anon_server_time INTEGER NOT NULL,PRIMARY KEY(key));
INSERT INTO k_anon VALUES(13291340603098283,replace('AdBid\nhttps://owner.example.com/\nhttps://owner.example.com/bidder.js\nhttps://ads.example.com/1','\n',char(10)),1,-9223372036854775805,-9223372036854775808);
INSERT INTO k_anon VALUES(13291340603098283,replace('NameReport\nhttps://owner.example.com/\nhttps://owner.example.com/bidder.js\nhttps://ads.example.com/1\ngroup2','\n',char(10)),0,-9223372036854775800,-9223372036854775808);
INSERT INTO k_anon VALUES(13291340603098283,replace('AdBid\nhttps://owner.example2.com/\nhttps://owner.example2.com/bidder.js\nhttps://ads.example2.com/1','\n',char(10)),1,-9223372036854775805,-9223372036854775800);
CREATE TABLE IF NOT EXISTS "interest_groups"(expiration INTEGER NOT NULL,last_updated INTEGER NOT NULL,next_update_after INTEGER NOT NULL,owner TEXT NOT NULL,joining_origin TEXT NOT NULL,exact_join_time INTEGER NOT NULL,name TEXT NOT NULL,priority DOUBLE NOT NULL,enable_bidding_signals_prioritization INTEGER NOT NULL,priority_vector TEXT NOT NULL,priority_signals_overrides TEXT NOT NULL,seller_capabilities TEXT NOT NULL,all_sellers_capabilities INTEGER NOT NULL,execution_mode INTEGER NOT NULL,joining_url TEXT NOT NULL,bidding_url TEXT NOT NULL,bidding_wasm_helper_url TEXT NOT NULL,update_url TEXT NOT NULL,trusted_bidding_signals_url TEXT NOT NULL,trusted_bidding_signals_keys TEXT NOT NULL,user_bidding_signals TEXT,ads_pb BLOB NOT NULL,ad_components_pb BLOB NOT NULL,ad_sizes TEXT NOT NULL,size_groups TEXT NOT NULL,auction_server_request_flags INTEGER NOT NULL,additional_bid_key BLOB NOT NULL,PRIMARY KEY(owner,name));
INSERT INTO interest_groups VALUES(13293932603076872,13291340603081533,-9223372036854775808,'https://owner.example.com','https://publisher.example.com',13291340603081533,'group1',0.0,0,'','','',0,0,'https://publisher.example.com/page1.html','https://owner.example.com/bidder.js','','https://owner.example.com/update','https://owner.example.com/signals','["group1"]','[["1","2"]]',X'0a2f0a1968747470733a2f2f6164732e6578616d706c652e636f6d2f311a125b2234222c2235222c6e756c6c2c2236225d',X'','','',0,X'');
INSERT INTO interest_groups VALUES(13293932603080090,13291340603089914,-9223372036854775808,'https://owner.example.com','https://publisher.example.com',13291340603089914,'group2',0.0,0,'','','',0,0,'https://publisher.example.com/page2.html','https://owner.example.com/bidder.js','','https://owner.example.com/update','https://owner.example.com/signals','["group2"]','[["1","3"]]',X'0a2f0a1968747470733a2f2f6164732e6578616d706c652e636f6d2f311a125b2234222c2235222c6e756c6c2c2236225d',X'','','',0,X'');
INSERT INTO interest_groups VALUES(13293932603052561,13291340603098283,-9223372036854775808,'https://owner.example.com','https://publisher.example.com',13291340603098283,'group3',0.0,0,'','','',0,0,'https://publisher.example.com/page3.html','https://owner.example.com/bidder.js','','https://owner.example.com/update','https://owner.example.com/signals','["group3"]','[["3","2"]]',X'0a2f0a1968747470733a2f2f6164732e6578616d706c652e636f6d2f311a125b2234222c2235222c6e756c6c2c2236225d',X'','','',0,X'');
CREATE INDEX win_history_index
ON win_history(owner, name, win_time DESC);
CREATE INDEX k_anon_last_referenced_time ON k_anon(last_referenced_time DESC);
CREATE INDEX interest_group_expiration ON interest_groups(expiration DESC, owner, name);
CREATE INDEX interest_group_owner ON interest_groups(owner,expiration DESC,next_update_after ASC,name);
CREATE INDEX interest_group_joining_origin ON interest_groups(joining_origin, expiration DESC, owner, name);
COMMIT;
