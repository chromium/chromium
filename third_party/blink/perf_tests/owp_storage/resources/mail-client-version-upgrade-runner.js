/**
 * Performs the database upgrade by removing and adding indexes and object
 * store.
 * *
 * @param {IDBDatabase} db The IndexedDB database instance.
 * @param {IDBTransaction} transaction The upgrade transaction.
 */
const performDatabaseUpgrade = (db, transaction) => {
  // Remove/add indexes during upgrade
  const messagesStore = transaction.objectStore(messageStoreName);

  if (messagesStore.indexNames.contains('isImportant')) {
    messagesStore.deleteIndex('isImportant');
    messagesStore.deleteIndex('parentFolderAndSortTime');
  }

  messagesStore.createIndex('isImportant', 'metaData.isImportant');
  messagesStore.createIndex(
      'parentFolderAndSortTime', ['parentFolderId', 'metaData.timestamp']);

  const conversationsStore = transaction.objectStore(conversationStoreName);

  if (conversationsStore.indexNames.contains('messageIdToConversationId')) {
    conversationsStore.deleteIndex('messageIdToConversationId');
  }

  conversationsStore.createIndex(
      'messageIdToConversationId', 'messageIds',
      {multiEntry: true, unique: true});

  // Remove/add object stores during upgrade
  if (db.objectStoreNames.contains('TempObjectStore')) {
    db.deleteObjectStore('TempObjectStore');
  }
  db.createObjectStore('TempObjectStore', {keyPath: 'id'});
};

indexedDB.databases().then(databases => {
  let currentDatabase = databases.find(db => db.name === databaseName);
  let currentVersion = currentDatabase ? currentDatabase.version : 1;
  const upgradeVersion = currentVersion + 1;
  // Open the database with a higher version to trigger upgrade
  const openRequest = window.indexedDB.open(databaseName, upgradeVersion);

  openRequest.onupgradeneeded = async (event) => {
    const db = openRequest.result;
    const transaction = event.target.transaction;

    try {
      performDatabaseUpgrade(db, transaction);
      // Don't call reportDone() here - let onsuccess handle it
    } catch (error) {
      reportError('Error during database upgrade: ', error);
    }
  };

  openRequest.onsuccess = async () => {
    const db = openRequest.result;
    const currentVersion = db.version;
    if (db.version != upgradeVersion) {
      reportError(
          'Database version mismatch: ',
          {expected: upgradeVersion, actual: currentVersion});
    }
    reportDone(currentVersion);
  };

  openRequest.onerror = (event) => {
    reportError('Error opening database: ', event.target.error);
  };
});
