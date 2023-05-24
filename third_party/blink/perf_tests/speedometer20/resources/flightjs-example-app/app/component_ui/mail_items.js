'use strict';

define(

  [
    'flight/lib/component',
    './with_select'
  ],

  function(defineComponent, withSelect) {

    return defineComponent(mailItems, withSelect);

    function mailItems() {

      this.defaultAttrs({
        deleteFolder: 'trash',
        selectedClass: 'selected',
        allowMultiSelect: true,
        selectionChangedEvent: 'uiMailItemSelectionChanged',
        selectedMailItems: [],
        selectedFolders: [],
        //selectors
        itemSelector: 'tr.mail-item',
        selectedItemSelector: 'tr.mail-item.selected'
      });

      this.renderItems = function(ev, data) {
        this.select('itemContainerSelector').html(data.markup);
        //new items, so no selections
        this.trigger('uiMailItemSelectionChanged', {selectedIds: []});
      }

      this.updateMailItemSelections = function(ev, data) {
        this.attr.selectedMailItems = data.selectedIds;
      }

      this.updateFolderSelections = function(ev, data) {
        this.attr.selectedFolders = data.selectedIds;
      }

      this.requestDeletion = function() {
        this.trigger('uiMoveItemsRequested', {
          itemIds: this.attr.selectedMailItems,
          fromFolder: this.attr.selectedFolders[0],
          toFolder: this.attr.deleteFolder
        });
      };

      this.after('initialize', function() {
        this.on(document, 'dataMailItemsServed', this.renderItems);
        this.on(document, 'uiDeleteMail', this.requestDeletion);

        this.on('uiMailItemSelectionChanged', this.updateMailItemSelections);
        this.on(document, 'uiFolderSelectionChanged', this.updateFolderSelections);

        this.trigger('uiMailItemsRequested');
      });
    }
  }
);
