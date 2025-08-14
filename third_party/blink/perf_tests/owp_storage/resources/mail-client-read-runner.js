const openRequest = self.indexedDB.open(databaseName);
openRequest.onsuccess = async () => {
  const db = openRequest.result;
  try {
    await performReadOperations(db);
    reportDone();
  } catch (error) {
    reportError(error);
  }
};
openRequest.onerror = reportError;

// Uses constants defined in mail-client-config.js.
const performReadOperations = async (db) => {
  // The number of rows loaded to display a typical "page" of data.
  const loadCount = 200;

  // Folder view:
  // - Get the conversation count: IDBObjectStore.count().
  // - Load the first few conversations: IDBObjectStore.getAll().
  // - Load the first few messages per conversation: IDBObjectStore.get().
  {
    const keyRange = IDBKeyRange.lowerBound([targetFolderId]);
    const store = getStore(db, conversationStoreName);
    await wrapRequest(store.count(keyRange));
    const conversations = await wrapRequest(store.getAll(keyRange, loadCount));
    if (conversations.length !== loadCount) {
      throw new Error(`Expected ${loadCount} conversations, but got ${
          conversations.length}`);
    }
    await getMessagesByConversation(
        db, conversations, /*messagesPerConversation:*/ 3);
  }

  // Filtering:
  // - Get flagged message count: IDBIndex.count().
  // - Load the first few flagged messages: IDBIndex.getAll().
  {
    const keyRange = IDBKeyRange.only(1);
    const index = getIndex(db, messageStoreName, 'flagStatus');
    await wrapRequest(index.count(keyRange));
    const flaggedMessages =
        await wrapRequest(index.getAll(keyRange, loadCount));
    if (flaggedMessages.length !== loadCount) {
      throw new Error(`Expected ${loadCount} flagged messages, but got ${
          flaggedMessages.length}`);
    }
  }

  // Searching:
  // - Get unique recipients of unread messages: IDBIndex.openCursor().
  // - Get IDs of messages CC'ed to a specific prefix: IDBIndex.getAllKeys().
  {
    await getUniqueRecipientsOfUnreadMessages(db, loadCount);
    // The lower bound of the range is the prefix to search for and the upper
    // bound is the next possible key after the prefix.
    const keyRange = IDBKeyRange.bound('cc0-3-1', 'cc0-3-2', false, true);
    const index = getIndex(db, messageStoreName, 'cc');
    const messageIds = await wrapRequest(index.getAllKeys(keyRange, loadCount));
    if (messageIds.length !== loadCount) {
      throw new Error(
          `Expected ${loadCount} message IDs, but got ${messageIds.length}`);
    }
  }
};

const getMessagesByConversation =
    async (db, conversations, messagesPerConversation) => {
  const messages = {};
  const store = getStore(db, messageStoreName);
  await Promise.all(conversations.map(async (conversation) => {
    const messageIds =
        conversation.messageIds.slice(0, messagesPerConversation);
    messages[conversation.id] = await bulkGetIDBValues(store, messageIds);
  }));
  return messages;
};

const getUniqueRecipientsOfUnreadMessages = async (db, count) => {
  const index = getIndex(db, messageStoreName, 'to');
  const uniqueRecipients = [];
  return new Promise((resolve, reject) => {
    const request = index.openCursor(/*range=*/ null, 'nextunique');
    request.onsuccess = (event) => {
      const cursor = event.target.result;
      if (cursor) {
        if (cursor.value.metaData.isRead === 0) {
          uniqueRecipients.push(cursor.key);
          if (uniqueRecipients.length == count) {
            resolve(uniqueRecipients);
          }
        }
        cursor.continue();
      } else {
        reject(`Expected ${count} unique recipients, but got ${
            uniqueRecipients.length}`);
      }
    };
    request.onerror = () =>
        reject(`Failed to get unique recipients: ${request.error}`);
  });
};
