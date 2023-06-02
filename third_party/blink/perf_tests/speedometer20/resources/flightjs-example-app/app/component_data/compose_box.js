'use strict';

define(

  [
    'flight/lib/component',
    'components/mustache/mustache',
    'app/data',
    'app/templates'
  ],

  function(defineComponent, Mustache, dataStore, templates) {
    return defineComponent(composeBox);

    function composeBox() {

      this.defaultAttrs({
        dataStore: dataStore,
        recipientHintId: 'recipient_hint',
        subjectHint: 'Subject',
        messageHint: 'Message',
        toHint: 'To',
        forwardPrefix: 'Fw',
        replyPrefix: 'Re'
      });

      this.serveComposeBox = function(ev, data) {
        this.trigger("dataComposeBoxServed", {
          markup: this.renderComposeBox(data.type, data.relatedMailId),
          type: data.type});
      };

      this.getSubject = function(type, relatedMailId) {
        var relatedMail = this.attr.dataStore.mail.filter(function(each) {
          return each.id ==  relatedMailId;
        })[0];

        var subject = relatedMail && relatedMail.subject;

        var subjectLookup = {
          newMail: this.attr.subjectHint,
          forward: this.attr.forwardPrefix + ": " + subject,
          reply: this.attr.replyPrefix + ": " + subject
        }

        return subjectLookup[type];
      };

      this.renderComposeBox = function(type, relatedMailId) {
        var recipientId = this.getRecipientId(type, relatedMailId);
        var contacts = this.attr.dataStore.contacts.map(function(contact) {
          contact.recipient = (contact.id == recipientId);
          return contact;
        });

        return Mustache.render(templates.composeBox, {
          newMail: type == 'newMail',
          reply: type == 'reply',
          subject: this.getSubject(type, relatedMailId),
          message: this.attr.messageHint,
          contacts: contacts
        });
      };

      this.getRecipientId = function(type, relatedMailId) {
        var relatedMail = (type == 'reply') && this.attr.dataStore.mail.filter(function(each) {
          return each.id ==  relatedMailId;
        })[0];

        return relatedMail && relatedMail.contact_id || this.attr.recipientHintId;
      };


      this.send = function(ev, data) {
        this.attr.dataStore.mail.push({
          id: String(Date.now()),
          contact_id: data.to_id,
          folders: ["sent"],
          time: Date.now(),
          subject: data.subject,
          message: data.message
        });
        this.trigger('dataMailItemsRefreshRequested', {folder: data.currentFolder});
      };

      this.after("initialize", function() {
        this.on("uiComposeBoxRequested", this.serveComposeBox);
        this.on("uiSendRequested", this.send);
      });
    }

  }
);
