'use strict';

define(

  [
    'flight/lib/component',
    'components/mustache/mustache',
    'app/data',
    'app/templates'
  ],

  function(defineComponent, Mustache, dataStore, templates) {
    return defineComponent(moveTo);

    function moveTo() {

      this.defaultAttrs({
        dataStore: dataStore
      });

      this.serveAvailableFolders = function(ev, data) {
        this.trigger("dataMoveToItemsServed", {
          markup: this.renderFolderSelector(this.getOtherFolders(data.folder))
        })
      };

      this.renderFolderSelector = function(items) {
        return Mustache.render(templates.moveToSelector, {moveToItems: items});
      };

      this.moveItems = function(ev, data) {
        var itemsToMoveIds = data.itemIds
        this.attr.dataStore.mail.forEach(function(item) {
          if (itemsToMoveIds.indexOf(item.id) > -1) {
            item.folders = [data.toFolder];
          }
        });
        this.trigger('dataMailItemsRefreshRequested', {folder: data.fromFolder});
      };

      this.getOtherFolders = function(folder) {
        return this.attr.dataStore.folders.filter(function(e) {return e != folder});
      };

      this.after("initialize", function() {
        this.on("uiAvailableFoldersRequested", this.serveAvailableFolders);
        this.on("uiMoveItemsRequested", this.moveItems);
      });
    }

  }
);
