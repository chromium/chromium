'use strict';

define(

  [
    'flight/lib/component',
    './with_select'
  ],

  function(defineComponent, withSelect) {

    return defineComponent(folders, withSelect);

    function folders() {

      this.defaultAttrs({
        selectedClass: 'selected',
        selectionChangedEvent: 'uiFolderSelectionChanged',

        //selectors
        itemSelector: 'li.folder-item',
        selectedItemSelector: 'li.folder-item.selected'
      });

      this.fetchMailItems = function(ev, data) {
          this.trigger('uiMailItemsRequested', {folder: data.selectedIds[0]});
      }

      this.after('initialize', function() {
        this.on('uiFolderSelectionChanged', this.fetchMailItems);
      });
    }
  }
);
