'use strict';

define(

  [
    'flight/lib/component',
    './with_select'
  ],

  function(defineComponent, withSelect) {

    return defineComponent(moveToSelector, withSelect);

    function moveToSelector() {

      this.defaultAttrs({
        selectionChangedEvent: 'uiMoveToSelectionChanged',
        selectedMailItems: [],
        selectedFolders: [],
        //selectors
        itemSelector: 'li.move-to-item',
        selectedItemSelector: 'li.move-to-item.selected'
      });

      this.requestSelectorWidget = function(ev, data) {
        this.trigger('uiAvailableFoldersRequested', {
          folder: this.attr.selectedFolders[0]
        })
      };

      this.launchSelector = function(ev, data) {
        var controlPosition = $(this.attr.moveActionSelector).offset();
        this.$node.html(data.markup).show().css({
          left: controlPosition.left,
          top: controlPosition.top + $(this.attr.moveActionSelector).outerHeight(),
          width: $(this.attr.moveActionSelector).outerWidth()
        });
        window.setTimeout(
          (function() {
            this.on(document, 'click', this.hideSelector)
          }).bind(this), 0);
      };

      this.hideSelector = function() {
        this.off(document, 'click', this.hideSelector);
        this.$node.hide();
      }

      this.updateMailItemSelections = function(ev, data) {
        this.attr.selectedMailItems = data.selectedIds;
      }

      this.updateFolderSelections = function(ev, data) {
        this.attr.selectedFolders = data.selectedIds;
      }

      this.requestMoveTo = function(ev, data) {
        this.trigger('uiMoveItemsRequested', {
          itemIds: this.attr.selectedMailItems,
          fromFolder: this.attr.selectedFolders[0],
          toFolder: data.selectedIds[0]
        });
        this.$node.hide();
      };

      this.after('initialize', function() {
        //show selector widget
        this.on(document, 'uiMoveMail', this.requestSelectorWidget);
        this.on(document, 'dataMoveToItemsServed', this.launchSelector);
        //listen for other selections
        this.on(document, 'uiMailItemSelectionChanged', this.updateMailItemSelections);
        this.on(document, 'uiFolderSelectionChanged', this.updateFolderSelections);
        //move items
        this.on('uiMoveToSelectionChanged', this.requestMoveTo);

      });
    }
  }
);
