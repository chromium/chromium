'use strict';

define(

  [
    'flight/lib/component',
    'components/mustache/mustache',
    'app/data',
    'app/templates'
  ],

  function(defineComponent, Mustache, dataStore, templates) {
    return defineComponent(mailItems);

    function mailItems() {

      this.defaultAttrs({
        folder: 'inbox',
        dataStore: dataStore
      });

      this.serveMailItems = function(ev, data) {
        var folder = (data && data.folder) || this.attr.folder;
        this.trigger("dataMailItemsServed", {markup: this.renderItems(this.assembleItems(folder))})
      };

      this.renderItems = function(items) {
        return Mustache.render(templates.mailItem, {mailItems: items});
      };

      this.assembleItems = function(folder) {
        var items = [];

        this.attr.dataStore.mail.forEach(function(each) {
          if (each.folders && each.folders.indexOf(folder) > -1) {
            items.push(this.getItemForView(each));
          }
        }, this);

        return items;
      };

      this.getItemForView = function(itemData) {
        var thisItem, thisContact, msg

        thisItem = {id: itemData.id, important: itemData.important};

        thisContact = this.attr.dataStore.contacts.filter(function(contact) {
          return contact.id == itemData.contact_id
        })[0];
        thisItem.name = [thisContact.firstName, thisContact.lastName].join(' ');

        var subj = itemData.subject;
        thisItem.formattedSubject = subj.length > 70 ? subj.slice(0, 70) + "..." : subj;

        var msg = itemData.message;
        thisItem.formattedMessage = msg.length > 70 ? msg.slice(0, 70) + "..." : msg;

        return thisItem;
      };

      this.after("initialize", function() {
        this.on("uiMailItemsRequested", this.serveMailItems);
        this.on("dataMailItemsRefreshRequested", this.serveMailItems);
      });
    }
  }
);
